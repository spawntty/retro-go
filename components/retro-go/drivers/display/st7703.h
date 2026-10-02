/* ST7703 MIPI-DSI backend. WHY2025 wiring and initialization follow BadgeVMS. */
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7703.h>
#include <esp_ldo_regulator.h>
#include <driver/ppa.h>
#include <esp_heap_caps.h>
#include <esp_private/esp_cache_private.h>
#include "targets/why2025/panel.h"

/* This backend uses degrees counterclockwise, not SPI MADCTL bit values. */
#if RG_SCREEN_ROTATION != 0 && RG_SCREEN_ROTATION != 90
#error "ST7703 supports RG_SCREEN_ROTATION 0 or 90 degrees CCW"
#endif
#if RG_SCREEN_WIDTH != RG_SCREEN_HEIGHT
#error "ST7703 rotation currently requires a square panel"
#endif

#define LCD_ACCESS_MODE 0
#define LCD_BUFFER_LENGTH (RG_SCREEN_WIDTH * 4)

static esp_ldo_channel_handle_t lcd_phy_power;
static esp_lcd_dsi_bus_handle_t lcd_bus;
static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static uint16_t lcd_buffer[LCD_BUFFER_LENGTH];
#if RG_SCREEN_ROTATION == 90
static ppa_client_handle_t lcd_ppa;
static uint16_t *lcd_shadow;
static uint16_t *lcd_rotated;
static size_t lcd_rotation_buffer_size;
static int lcd_dirty_left, lcd_dirty_top, lcd_dirty_right, lcd_dirty_bottom;
#endif
static int lcd_left, lcd_top, lcd_width, lcd_height;
static size_t lcd_position;
static const st7703_lcd_init_cmd_t lcd_init_commands[] = CUSTOM_INIT_CMDS();

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
    /* Keep DPI copies synchronous so the rotation buffer can be reused after
     * draw_bitmap returns. Single scanout buffering can still tear. */
    esp_lcd_dpi_panel_config_t dpi = ST7703_720_720_PANEL_60HZ_DPI_CONFIG();
    st7703_vendor_config_t vendor = {
        .mipi_config = {.dsi_bus = lcd_bus, .dpi_config = &dpi},
        .init_cmds = lcd_init_commands, .init_cmds_size = RG_COUNT(lcd_init_commands),
    };
    esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = RG_GPIO_LCD_RST, .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, .vendor_config = &vendor,
        .flags.reset_active_high = 1,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7703(lcd_io, &panel, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
#if RG_SCREEN_ROTATION == 90
    size_t alignment;
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &alignment));
    size_t bytes = RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * sizeof(uint16_t);
    lcd_rotation_buffer_size = (bytes + alignment - 1) / alignment * alignment;
    lcd_shadow = heap_caps_aligned_calloc(alignment, 1, lcd_rotation_buffer_size,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lcd_rotated = heap_caps_aligned_calloc(alignment, 1, lcd_rotation_buffer_size,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    RG_ASSERT(lcd_shadow && lcd_rotated, "Cannot allocate LCD rotation buffers");
    ppa_client_config_t ppa = {.oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1};
    ESP_ERROR_CHECK(ppa_register_client(&ppa, &lcd_ppa));
    lcd_dirty_left = RG_SCREEN_WIDTH;
    lcd_dirty_top = RG_SCREEN_HEIGHT;
    lcd_dirty_right = lcd_dirty_bottom = 0;
#endif
}

static void lcd_sync(void)
{
#if RG_SCREEN_ROTATION == 90
    if (lcd_dirty_right <= lcd_dirty_left || lcd_dirty_bottom <= lcd_dirty_top)
        return;

    int width = lcd_dirty_right - lcd_dirty_left;
    int height = lcd_dirty_bottom - lcd_dirty_top;
    /* Pack the rotated rectangle at offset zero in a separate buffer. This
     * avoids PPA writes into live scanout memory and preserves unchanged pixels
     * when the dirty bounding box includes gaps between partial updates.
     * The PPA driver handles source writeback and destination invalidation. */
    ppa_srm_oper_config_t rotation = {
        .in = {
            .buffer = lcd_shadow,
            .pic_w = RG_SCREEN_WIDTH, .pic_h = RG_SCREEN_HEIGHT,
            .block_w = width, .block_h = height,
            .block_offset_x = lcd_dirty_left, .block_offset_y = lcd_dirty_top,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = lcd_rotated, .buffer_size = lcd_rotation_buffer_size,
            .pic_w = height, .pic_h = width,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = 1.0f, .scale_y = 1.0f,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(lcd_ppa, &rotation));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel,
        lcd_dirty_top, RG_SCREEN_WIDTH - lcd_dirty_right,
        lcd_dirty_bottom, RG_SCREEN_WIDTH - lcd_dirty_left, lcd_rotated));
    lcd_dirty_left = RG_SCREEN_WIDTH;
    lcd_dirty_top = RG_SCREEN_HEIGHT;
    lcd_dirty_right = lcd_dirty_bottom = 0;
#endif
}

static void lcd_deinit(void)
{
    lcd_sync();
#if RG_SCREEN_ROTATION == 90
    ESP_ERROR_CHECK(ppa_unregister_client(lcd_ppa));
    heap_caps_free(lcd_shadow);
    heap_caps_free(lcd_rotated);
    lcd_ppa = NULL;
    lcd_shadow = lcd_rotated = NULL;
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_del(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_io_del(lcd_io));
    ESP_ERROR_CHECK(esp_lcd_del_dsi_bus(lcd_bus));
    ESP_ERROR_CHECK(esp_ldo_release_channel(lcd_phy_power));
    lcd_panel = NULL;
    lcd_io = NULL;
    lcd_bus = NULL;
    lcd_phy_power = NULL;
}

static void lcd_set_backlight(float percent)
{
    (void)percent; // BadgeVMS does not expose backlight control.
}

static void lcd_set_window(int left, int top, int width, int height)
{
    RG_ASSERT(left >= 0 && top >= 0 && width > 0 && height > 0 &&
              left + width <= RG_SCREEN_WIDTH && top + height <= RG_SCREEN_HEIGHT,
              "Invalid LCD window");
    lcd_left = left;
    lcd_top = top;
    lcd_width = width;
    lcd_height = height;
    lcd_position = 0;
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    RG_ASSERT(length <= LCD_BUFFER_LENGTH, "LCD buffer too small");
    return lcd_buffer;
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    /* clear_rect may submit chunks that end midway through a window row. */
    RG_ASSERT(lcd_position + length <= (size_t)lcd_width * lcd_height, "LCD window overflow");
    while (length)
    {
        int x = lcd_position % lcd_width;
        int y = lcd_position / lcd_width;
        int width = RG_MIN(length, (size_t)(lcd_width - x));
        int rows = x == 0 && length >= lcd_width ? length / lcd_width : 1;
        size_t count = (size_t)width * rows;
#if RG_SCREEN_ROTATION == 90
        for (int row = 0; row < rows; ++row)
            memcpy(lcd_shadow + (lcd_top + y + row) * RG_SCREEN_WIDTH + lcd_left + x,
                   buffer + row * width, width * sizeof(uint16_t));
        lcd_dirty_left = RG_MIN(lcd_dirty_left, lcd_left + x);
        lcd_dirty_top = RG_MIN(lcd_dirty_top, lcd_top + y);
        lcd_dirty_right = RG_MAX(lcd_dirty_right, lcd_left + x + width);
        lcd_dirty_bottom = RG_MAX(lcd_dirty_bottom, lcd_top + y + rows);
#else
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel,
            lcd_left + x, lcd_top + y, lcd_left + x + width, lcd_top + y + rows, buffer));
#endif
        buffer += count;
        length -= count;
        lcd_position += count;
    }
}
