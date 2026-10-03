/* ST7703 MIPI-DSI backend. WHY2025 wiring and initialization follow BadgeVMS. */
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7703.h>
#include <esp_ldo_regulator.h>
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <driver/ppa.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "targets/why2025/panel.h"

/* This backend uses degrees counterclockwise, not SPI MADCTL bit values. */
#if RG_SCREEN_ROTATION != 0 && RG_SCREEN_ROTATION != 90
#error "ST7703 supports RG_SCREEN_ROTATION 0 or 90 degrees CCW"
#endif
#if RG_SCREEN_WIDTH != RG_SCREEN_HEIGHT
#error "ST7703 rotation currently requires a square panel"
#endif
#if RG_SCREEN_PIXEL_FORMAT != 1
#error "ST7703 scanout is little-endian RGB565"
#endif

/* The panel runs in DSI video mode and has no frame memory, so MADCTL cannot
 * swap axes. Game frames are scaled and rotated by the PPA into the hidden one
 * of two DPI scanout buffers, then flipped. CPU writes (menus, borders, clears)
 * go to both buffers so they survive flips. */
#define LCD_ACCESS_MODE 2
#define LCD_NUM_FBS 2
// The PPA scales in 1/16 steps; rg_display snaps the viewport to these.
#define LCD_SCALE_STEPS 16

static esp_ldo_channel_handle_t lcd_phy_power;
static esp_lcd_dsi_bus_handle_t lcd_bus;
static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static uint16_t *lcd_fbs[LCD_NUM_FBS];
static const size_t lcd_fb_size = RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * sizeof(uint16_t);
static ppa_client_handle_t lcd_ppa;
static uint16_t *lcd_scratch;
static size_t lcd_scratch_size;
static SemaphoreHandle_t lcd_flip_done;
static volatile int lcd_scanning; // Buffer the DPI DMA is scanning out
static volatile int lcd_selected; // Buffer chosen by the last flip
static int lcd_dirty_top = RG_SCREEN_HEIGHT, lcd_dirty_bottom;
static const st7703_lcd_init_cmd_t lcd_init_commands[] = CUSTOM_INIT_CMDS();

IRAM_ATTR
static bool lcd_on_frame_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    // The DMA picked the buffer for the next refresh just before this callback.
    // A flip racing that pick by a few cycles is reported one frame early,
    // which can only cause a single torn frame.
    if (lcd_scanning != lcd_selected)
    {
        lcd_scanning = lcd_selected;
        xSemaphoreGiveFromISR(lcd_flip_done, &woken);
    }
    return woken == pdTRUE;
}

static void lcd_init(void)
{
    esp_ldo_channel_config_t power = {.chan_id = 3, .voltage_mv = 2500};
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&power, &lcd_phy_power));
    esp_lcd_dsi_bus_config_t bus = {
        .bus_id = 0, .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT, .lane_bit_rate_mbps = 1000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus, &lcd_bus));
    esp_lcd_dbi_io_config_t io = {
        .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(lcd_bus, &io, &lcd_io));
    esp_lcd_dpi_panel_config_t dpi = ST7703_720_720_PANEL_60HZ_DPI_CONFIG();
    dpi.flags.use_dma2d = false;
    dpi.num_fbs = LCD_NUM_FBS;
    st7703_vendor_config_t vendor = {
        .mipi_config = {.dsi_bus = lcd_bus, .dpi_config = &dpi},
        .init_cmds = lcd_init_commands, .init_cmds_size = RG_COUNT(lcd_init_commands),
    };
    esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = RG_GPIO_LCD_RST, .bits_per_pixel = 16,
        .rgb_ele_order = RG_SCREEN_RGB_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .vendor_config = &vendor,
        .flags.reset_active_high = 1,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7703(lcd_io, &panel, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcd_panel, LCD_NUM_FBS,
        (void **)&lcd_fbs[0], (void **)&lcd_fbs[1]));

    lcd_flip_done = xSemaphoreCreateBinary();
    RG_ASSERT(lcd_flip_done, "Cannot create LCD flip semaphore");
    lcd_scanning = lcd_selected = 0;
    esp_lcd_dpi_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = lcd_on_frame_done};
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(lcd_panel, &callbacks, NULL));

    ppa_client_config_t ppa = {.oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1};
    ESP_ERROR_CHECK(ppa_register_client(&ppa, &lcd_ppa));
    lcd_dirty_top = RG_SCREEN_HEIGHT;
    lcd_dirty_bottom = 0;
}

static void lcd_sync(void)
{
    if (lcd_dirty_top >= lcd_dirty_bottom)
        return;
    // Publish CPU writes to the DMA (scanout and PPA). Rows are physical.
    size_t offset = (size_t)lcd_dirty_top * RG_SCREEN_WIDTH;
    size_t length = (size_t)(lcd_dirty_bottom - lcd_dirty_top) * RG_SCREEN_WIDTH * sizeof(uint16_t);
    for (int i = 0; i < LCD_NUM_FBS; ++i)
        ESP_ERROR_CHECK(esp_cache_msync(lcd_fbs[i] + offset, length,
            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED));
    lcd_dirty_top = RG_SCREEN_HEIGHT;
    lcd_dirty_bottom = 0;
}

static void lcd_deinit(void)
{
    lcd_sync();
    ESP_ERROR_CHECK(ppa_unregister_client(lcd_ppa));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_del(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_io_del(lcd_io));
    ESP_ERROR_CHECK(esp_lcd_del_dsi_bus(lcd_bus));
    ESP_ERROR_CHECK(esp_ldo_release_channel(lcd_phy_power));
    vSemaphoreDelete(lcd_flip_done);
    heap_caps_free(lcd_scratch);
    lcd_scratch = NULL;
    lcd_scratch_size = 0;
    lcd_flip_done = NULL;
    lcd_ppa = NULL;
    lcd_fbs[0] = lcd_fbs[1] = NULL;
    lcd_panel = NULL;
    lcd_io = NULL;
    lcd_bus = NULL;
    lcd_phy_power = NULL;
}

static void lcd_set_backlight(float percent)
{
    (void)percent; // BadgeVMS does not expose backlight control.
}

// Logical (x,y) -> physical (y,N-1-x) for 90 degrees CCW.
static inline size_t lcd_offset(int x, int y)
{
#if RG_SCREEN_ROTATION == 90
    return (size_t)(RG_SCREEN_WIDTH - 1 - x) * RG_SCREEN_WIDTH + y;
#else
    return (size_t)y * RG_SCREEN_WIDTH + x;
#endif
}

// Clips a logical rectangle to the panel and marks its physical rows dirty.
static bool lcd_clip(int *left, int *top, int *width, int *height)
{
    int right = RG_MIN(*left + *width, RG_SCREEN_WIDTH);
    int bottom = RG_MIN(*top + *height, RG_SCREEN_HEIGHT);
    *left = RG_MAX(*left, 0);
    *top = RG_MAX(*top, 0);
    *width = right - *left;
    *height = bottom - *top;
    if (*width <= 0 || *height <= 0)
        return false;
#if RG_SCREEN_ROTATION == 90
    lcd_dirty_top = RG_MIN(lcd_dirty_top, RG_SCREEN_WIDTH - right);
    lcd_dirty_bottom = RG_MAX(lcd_dirty_bottom, RG_SCREEN_WIDTH - *left);
#else
    lcd_dirty_top = RG_MIN(lcd_dirty_top, *top);
    lcd_dirty_bottom = RG_MAX(lcd_dirty_bottom, bottom);
#endif
    return true;
}

static inline uint16_t lcd_pixel(const uint16_t *src, bool swap)
{
    return swap ? (*src >> 8) | (*src << 8) : *src;
}

// stride is in bytes. Each physical row segment is written to the first buffer
// sequentially, then copied to the others.
static void lcd_write_rect(int left, int top, int width, int height, const uint16_t *src, size_t stride, bool swap)
{
    int x0 = left, y0 = top;
    if (!lcd_clip(&left, &top, &width, &height))
        return;
    src = (const void *)src + (top - y0) * stride + (left - x0) * sizeof(uint16_t);
#if RG_SCREEN_ROTATION == 90
    // Physical rows are logical columns.
    for (int x = 0; x < width; ++x)
    {
        size_t offset = lcd_offset(left + x, top);
        uint16_t *row = lcd_fbs[0] + offset;
        const uint16_t *col = src + x;
        for (int y = 0; y < height; ++y, col = (const void *)col + stride)
            row[y] = lcd_pixel(col, swap);
        for (int i = 1; i < LCD_NUM_FBS; ++i)
            memcpy(lcd_fbs[i] + offset, row, height * sizeof(uint16_t));
    }
#else
    for (int y = 0; y < height; ++y)
    {
        size_t offset = lcd_offset(left, top + y);
        uint16_t *row = lcd_fbs[0] + offset;
        const uint16_t *line = (const void *)src + y * stride;
        for (int x = 0; x < width; ++x)
            row[x] = lcd_pixel(line + x, swap);
        for (int i = 1; i < LCD_NUM_FBS; ++i)
            memcpy(lcd_fbs[i] + offset, row, width * sizeof(uint16_t));
    }
#endif
}

static void lcd_fill_rect(int left, int top, int width, int height, uint16_t color)
{
    if (!lcd_clip(&left, &top, &width, &height))
        return;
#if RG_SCREEN_ROTATION == 90
    int rows = width, length = height;
#else
    int rows = height, length = width;
#endif
    for (int r = 0; r < rows; ++r)
    {
#if RG_SCREEN_ROTATION == 90
        size_t offset = lcd_offset(left + r, top);
#else
        size_t offset = lcd_offset(left, top + r);
#endif
        for (int i = 0; i < LCD_NUM_FBS; ++i)
        {
            uint16_t *row = lcd_fbs[i] + offset;
            for (int p = 0; p < length; ++p)
                row[p] = color;
        }
    }
}

// Reusable PSRAM buffer for frames the PPA cannot read directly.
static uint16_t *lcd_get_scratch(size_t pixels)
{
    size_t size = pixels * sizeof(uint16_t);
    if (size > lcd_scratch_size)
    {
        heap_caps_free(lcd_scratch);
        lcd_scratch = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        RG_ASSERT(lcd_scratch, "Cannot allocate LCD scratch buffer");
        lcd_scratch_size = size;
    }
    return lcd_scratch;
}

// Scales an RGB565 block by scale_*/LCD_SCALE_STEPS and rotates it into the
// hidden buffer with its top-left corner at logical (left,top). pic_w is the
// source stride in pixels. Blocks until the PPA is done; call lcd_flip() after.
static void lcd_draw_scaled(const void *pixels, int pic_w, int pic_h, int src_x, int src_y, int src_w, int src_h,
                            bool swap, int scale_x, int scale_y, int left, int top)
{
    // Same arithmetic as the PPA driver's output block size.
    int width = src_w * scale_x / LCD_SCALE_STEPS;
    int height = src_h * scale_y / LCD_SCALE_STEPS;
    RG_ASSERT(left >= 0 && top >= 0 && width > 0 && height > 0 &&
              left + width <= RG_SCREEN_WIDTH && top + height <= RG_SCREEN_HEIGHT, "Invalid LCD region");

    // The previous flip must reach scanout before its old buffer is reused.
    while (lcd_scanning != lcd_selected)
        xSemaphoreTake(lcd_flip_done, pdMS_TO_TICKS(100));
    // The PPA invalidates the destination rows, so CPU writes must land first.
    lcd_sync();

    ppa_srm_oper_config_t op = {
        .in = {
            .buffer = pixels, .pic_w = pic_w, .pic_h = pic_h,
            .block_w = src_w, .block_h = src_h,
            .block_offset_x = src_x, .block_offset_y = src_y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = lcd_fbs[!lcd_selected], .buffer_size = lcd_fb_size,
            .pic_w = RG_SCREEN_WIDTH, .pic_h = RG_SCREEN_HEIGHT,
#if RG_SCREEN_ROTATION == 90
            .block_offset_x = top, .block_offset_y = RG_SCREEN_WIDTH - left - width,
#else
            .block_offset_x = left, .block_offset_y = top,
#endif
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
#if RG_SCREEN_ROTATION == 90
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
#else
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
#endif
        .scale_x = (float)scale_x / LCD_SCALE_STEPS,
        .scale_y = (float)scale_y / LCD_SCALE_STEPS,
        .byte_swap = swap,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(lcd_ppa, &op));
}

// Presents the hidden buffer at the next refresh.
static void lcd_flip(void)
{
    int target = !lcd_selected;
    lcd_sync();
    // A DPI-owned pointer only selects that buffer (plus a one-row cache
    // writeback); IDF copies nothing.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, RG_SCREEN_WIDTH, 1, lcd_fbs[target]));
    lcd_selected = target;
}
