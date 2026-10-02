/* ST7703 MIPI-DSI backend. WHY2025 wiring and initialization follow BadgeVMS. */
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7703.h>
#include <esp_ldo_regulator.h>
#include "targets/why2025/panel.h"

#define LCD_ACCESS_MODE 0
#define LCD_BUFFER_LENGTH (RG_SCREEN_WIDTH * 4)

static esp_ldo_channel_handle_t lcd_phy_power;
static esp_lcd_dsi_bus_handle_t lcd_bus;
static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static uint16_t lcd_buffer[LCD_BUFFER_LENGTH];
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
    /* CPU copies are synchronous: Retro-Go can immediately reuse its line buffer.
     * The DPI driver owns the PSRAM framebuffer and performs cache writeback.
     * One framebuffer permits tearing; no claim of VSYNC synchronization here. */
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
}

static void lcd_sync(void)
{
    /* draw_bitmap completes the CPU copy and cache writeback before returning. */
}

static void lcd_deinit(void)
{
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
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel,
            lcd_left + x, lcd_top + y, lcd_left + x + width, lcd_top + y + rows, buffer));
        buffer += count;
        length -= count;
        lcd_position += count;
    }
}
