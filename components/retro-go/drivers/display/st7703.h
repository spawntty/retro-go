#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7703.h>
#include <esp_ldo_regulator.h>
#include <esp_log.h>
#include <string.h>

#ifndef RG_SCREEN_ROTATE
#define RG_SCREEN_ROTATE 0
#endif

static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
static QueueHandle_t lcd_buffers;
static void *framebuffer = NULL;
static bool use_framebuffer = false;
static int frame_dirty_lines = 0;

#define LCD_BUFFER_COUNT                (3)
#define MIPI_DSI_PHY_PWR_LDO_CHAN       (3)
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)
#define LCD_MIPI_DSI_BUS_ID             (0)
#define LCD_MIPI_DSI_LANE_NUM           (2)
#define LCD_MIPI_DSI_LANE_BITRATE_MBPS  (1000)

#ifdef RG_SCREEN_ST7703_INIT_CMDS
static const st7703_lcd_init_cmd_t st7703_init_cmds[] = RG_SCREEN_ST7703_INIT_CMDS();
#endif

static esp_err_t enable_dsi_phy_power(void)
{
    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    return esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
}

static void lcd_queue_draw(int x, int y, int width, int height, const uint16_t *buffer)
{
    if (!buffer || !lcd_panel || width <= 0 || height <= 0)
        return;
        
    // Bounds checking
    if (x < 0 || y < 0 || x + width > RG_SCREEN_WIDTH || y + height > RG_SCREEN_HEIGHT) {
        RG_LOGW("ST7703: Invalid bounds (x=%d, y=%d, w=%d, h=%d)", x, y, width, height);
        return;
    }
    
    // MIPI DSI draw_bitmap expects (x_start, y_start, x_end, y_end)
    esp_err_t ret = esp_lcd_panel_draw_bitmap(lcd_panel, x, y, x + width, y + height, buffer);
    if (ret != ESP_OK) {
        RG_LOGW("ST7703: draw_bitmap failed: %s", esp_err_to_name(ret));
    }
}

static void lcd_set_window(int left, int top, int width, int height)
{
    // For MIPI-DSI, we don't need to set windows like SPI displays
    // The actual drawing happens in lcd_send_buffer via lcd_queue_draw
    if (left < 0 || top < 0 || left + width > RG_SCREEN_WIDTH || top + height > RG_SCREEN_HEIGHT) {
        RG_LOGW("ST7703: Invalid window bounds (x=%d, y=%d, w=%d, h=%d)", left, top, width, height);
    }
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    uint16_t *buffer;
    if (xQueueReceive(lcd_buffers, &buffer, pdMS_TO_TICKS(2500)) != pdTRUE)
        RG_PANIC("ST7703 display");
    return buffer;
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (use_framebuffer && framebuffer && length > 0) {
        // For MIPI DSI with framebuffer, copy directly and trigger refresh
        static int current_y = 0; // source line index (unrotated space)
        const int W = RG_SCREEN_WIDTH;
        const int H = RG_SCREEN_HEIGHT;

        // How many complete/partial lines are present in 'buffer'
        int lines = (length + W - 1) / W;  // Round up

        if (current_y + lines > H) {
            // If the incoming chunk would overflow the frame, wrap (frame-based producers usually start at 0)
            current_y = 0;
        }

#if (RG_SCREEN_ROTATE == 1)
        // Optimized 90° CCW rotation with better cache locality
        uint16_t* fb = (uint16_t*)framebuffer;
        
        for (int line = 0; line < lines && (current_y + line) < H; line++) {
            const int sy = current_y + line;
            const size_t src_offset = line * W;
            const size_t pixels_to_copy = RG_MIN(W, length - src_offset);
            const uint16_t* src_line = buffer + src_offset;
            
            // Pre-calculate base destination column (dx = sy)
            const int dx = sy;
            
            // Process pixels in blocks for better cache performance
            const size_t block_size = 8;
            size_t sx;
            
            // Process full blocks
            for (sx = 0; sx + block_size <= pixels_to_copy; sx += block_size) {
                const int dy_base = (W - 1) - (int)sx;
                
                // Unroll inner loop for better performance
                fb[(size_t)(dy_base - 0) * W + dx] = src_line[sx + 0];
                fb[(size_t)(dy_base - 1) * W + dx] = src_line[sx + 1];
                fb[(size_t)(dy_base - 2) * W + dx] = src_line[sx + 2];
                fb[(size_t)(dy_base - 3) * W + dx] = src_line[sx + 3];
                fb[(size_t)(dy_base - 4) * W + dx] = src_line[sx + 4];
                fb[(size_t)(dy_base - 5) * W + dx] = src_line[sx + 5];
                fb[(size_t)(dy_base - 6) * W + dx] = src_line[sx + 6];
                fb[(size_t)(dy_base - 7) * W + dx] = src_line[sx + 7];
            }
            
            // Handle remaining pixels
            for (; sx < pixels_to_copy; sx++) {
                const int dy = (W - 1) - (int)sx;
                fb[(size_t)dy * W + dx] = src_line[sx];
            }
        }
#else
        // No rotation: copy line-by-line as before (handles partial last line)
        for (int line = 0; line < lines && (current_y + line) < H; line++) {
            const size_t line_offset = (size_t)(current_y + line) * W;
            const size_t src_offset  = (size_t)line * W;
            const size_t pixels_to_copy = RG_MIN(W, length - src_offset);

            if (src_offset < length && pixels_to_copy > 0) {
                memcpy((uint16_t*)framebuffer + line_offset,
                       buffer + src_offset,
                       pixels_to_copy * sizeof(uint16_t));
            }
        }
#endif

        current_y += lines;
        frame_dirty_lines += lines;

        // Trigger display refresh periodically or when enough lines accumulated
        if (frame_dirty_lines >= 10 || current_y >= H) {
            esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, W, H, framebuffer);
            frame_dirty_lines = 0;
        }

    } else if (length > 0) {
        // Fallback to drawing bitmap (no rotation here)
        int width = RG_MIN(RG_SCREEN_WIDTH, length);
        int height = 1;
        if (width == RG_SCREEN_WIDTH && length > RG_SCREEN_WIDTH) {
            height = RG_MIN((int)(length / RG_SCREEN_WIDTH), RG_SCREEN_HEIGHT);
        }
        lcd_queue_draw(0, 0, width, height, buffer);
    }

    xQueueSend(lcd_buffers, &buffer, portMAX_DELAY);
}

static void lcd_set_backlight(float percent)
{
    float level = RG_MIN(RG_MAX(percent / 100.0f, 0), 1.0f);
    
#if defined(RG_GPIO_LCD_BCKL) && (RG_GPIO_LCD_BCKL >= 0)
    // Use PWM for backlight control if available
    ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0x1FFF * level, 50, 0);
#endif
    
    RG_LOGI("ST7703: Backlight set to %d%%", (int)(100 * level));
}

static void lcd_sync(void)
{
    // Flush any pending framebuffer updates
    if (use_framebuffer && framebuffer && frame_dirty_lines > 0) {
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, framebuffer);
        frame_dirty_lines = 0;
    }
    // Wait for any pending DMA transfers to complete
    // For MIPI-DSI this is handled by the panel driver
}

static void lcd_set_rotation(int rotation)
{
    // ST7703 rotation is handled via init commands
    // This would require re-initialization with different MADCTL values
    RG_LOGW("ST7703: Runtime rotation not supported, use init commands");
}

static bool on_refresh_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    // Handle refresh completion if needed
    return true;
}

static void lcd_init(void)
{
    
    RG_LOGI("ST7703: Enabling MIPI DSI PHY power");
    ESP_ERROR_CHECK(enable_dsi_phy_power());
    
    RG_LOGI("ST7703: Creating MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = LCD_MIPI_DSI_BUS_ID,
        .num_data_lanes = LCD_MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = LCD_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));
    
    RG_LOGI("ST7703: Installing MIPI DSI LCD control panel");
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &lcd_io));
    
    RG_LOGI("ST7703: Installing ST7703 LCD control panel");
    
#ifdef RG_SCREEN_ST7703_DPI_CONFIG
    esp_lcd_dpi_panel_config_t dpi_config = RG_SCREEN_ST7703_DPI_CONFIG();
#else
    esp_lcd_dpi_panel_config_t dpi_config = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 47,
        .virtual_channel = 0,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = LCD_BUFFER_COUNT,
        .video_timing = {
            .h_size = RG_SCREEN_WIDTH,
            .v_size = RG_SCREEN_HEIGHT,
            .hsync_back_porch = 120,
            .hsync_pulse_width = 60,
            .hsync_front_porch = 106,
            .vsync_back_porch = 20,
            .vsync_pulse_width = 4,
            .vsync_front_porch = 20,
        },
        .flags.use_dma2d = true,
        .flags.disable_lp = false
    };
#endif
    
    st7703_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
        },
#ifdef RG_SCREEN_ST7703_INIT_CMDS
        .init_cmds = st7703_init_cmds,
        .init_cmds_size = sizeof(st7703_init_cmds) / sizeof(st7703_lcd_init_cmd_t),
#endif
    };
    
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 16,  // RGB565 - matches FRAMEBUFFER_BPP * 8 from BadgeVMS
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // WHY2025 badge specific (backwards RGB)
        .reset_gpio_num = RG_GPIO_LCD_RST,
        .vendor_config = &vendor_config,
        .flags.reset_active_high = 1,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7703(lcd_io, &lcd_dev_config, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    
    // Register event callbacks
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_refresh_done = on_refresh_done,
    };
    esp_lcd_dpi_panel_register_event_callbacks(lcd_panel, &cbs, NULL);
    
    // Try to get framebuffer from MIPI DSI panel first
    void *fb = NULL;
    esp_err_t fb_result = esp_lcd_dpi_panel_get_frame_buffer(lcd_panel, 1, &fb);
    
    if (fb_result == ESP_OK && fb != NULL) {
        RG_LOGI("ST7703: Using panel framebuffer at %p", fb);
        framebuffer = fb;
        use_framebuffer = true;
        frame_dirty_lines = 0;
        
        // Clear framebuffer to black
        memset(framebuffer, 0, RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * sizeof(uint16_t));
        
        // Test pattern: Fill with red for 1 second to verify framebuffer works
        uint16_t red = 0x001F;  // RGB565 red (note: BGR order for WHY2025)
        for (int i = 0; i < 100; i++) {  // Fill first 100 lines with red
            for (int j = 0; j < RG_SCREEN_WIDTH; j++) {
                ((uint16_t*)framebuffer)[i * RG_SCREEN_WIDTH + j] = red;
            }
        }
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, framebuffer);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Show test pattern for 2 seconds
        
        // Clear back to black
        memset(framebuffer, 0, RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * sizeof(uint16_t));
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, framebuffer);
    } else {
        RG_LOGI("ST7703: Panel framebuffer not available, using draw_bitmap");
        use_framebuffer = false;
    }
    
    // Initialize buffer management for line buffers
    lcd_buffers = xQueueCreate(LCD_BUFFER_COUNT, sizeof(uint16_t *));
    
    // Create simple DMA buffers for line-by-line drawing
    for (int i = 0; i < LCD_BUFFER_COUNT; i++) {
        uint16_t *buffer = rg_alloc(LCD_BUFFER_LENGTH * sizeof(uint16_t), MEM_DMA);
        if (!buffer) {
            RG_PANIC("ST7703: Failed to allocate buffer");
        }
        xQueueSend(lcd_buffers, &buffer, portMAX_DELAY);
    }
    
#if defined(RG_GPIO_LCD_BCKL) && (RG_GPIO_LCD_BCKL >= 0)
    // Initialize backlight PWM
    ledc_timer_config(&(ledc_timer_config_t){
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
    });
    ledc_channel_config(&(ledc_channel_config_t){
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = RG_GPIO_LCD_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    });
    ledc_fade_func_install(0);
#endif
    
    RG_LOGI("ST7703: Display initialization complete");
}

static void lcd_deinit(void)
{
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
        lcd_panel = NULL;
    }
    if (lcd_io) {
        esp_lcd_panel_io_del(lcd_io);
        lcd_io = NULL;
    }
    if (dsi_bus) {
        esp_lcd_del_dsi_bus(dsi_bus);
        dsi_bus = NULL;
    }
    if (lcd_buffers) {
        vQueueDelete(lcd_buffers);
        lcd_buffers = NULL;
    }
    RG_LOGI("ST7703: Display deinitialized");
}

const rg_display_driver_t rg_display_driver_st7703 = {
    .name = "st7703",
};