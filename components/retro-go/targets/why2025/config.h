/* Configuration for the WHY2025 Badge
 * Based on the ESP32-P4 target
 * Some of the configuration were taken from https://gitlab.com/why2025/team-badge/firmware/
 * command to build: python rg_tool.py --target why2025 build-img --no-networking
*/

// Target definition
#define RG_TARGET_NAME             "WHY2025"

// Storage
#define RG_STORAGE_ROOT               "/sd"
#define RG_STORAGE_SDMMC_HOST         1
#define RG_STORAGE_SDMMC_SPEED        SDMMC_FREQ_HIGHSPEED

// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video - ST7703 MIPI-DSI Configuration
#define RG_SCREEN_DRIVER            1   // 1 = ST7703 MIPI-DSI
#define RG_SCREEN_SPEED             0   // Not used for MIPI-DSI
#define RG_SCREEN_BACKLIGHT         0
#define RG_SCREEN_WIDTH             720
#define RG_SCREEN_HEIGHT            720
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}  // Left, Top, Right, Bottom
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}  // Left, Top, Right, Bottom

// ST7703 specific configuration using BONO config from BadgeVMS
#define RG_SCREEN_ST7703_DPI_CONFIG() \
    { \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT, \
        .dpi_clock_freq_mhz = 47, \
        .virtual_channel = 0, \
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565, \
        .num_fbs = 3, \
        .video_timing = { \
            .h_size = 720, \
            .v_size = 720, \
            .hsync_back_porch = 120, \
            .hsync_pulse_width = 60, \
            .hsync_front_porch = 106, \
            .vsync_back_porch = 20, \
            .vsync_pulse_width = 4, \
            .vsync_front_porch = 20, \
        }, \
        .flags.use_dma2d = true, \
        .flags.disable_lp = false \
    }

// ST7703 initialization commands using BONO config
#define RG_SCREEN_ST7703_INIT_CMDS() \
    { \
        {0xB9, (uint8_t[]){0xF1, 0x12, 0x83}, 3, 0}, \
        {0xB1, (uint8_t[]){0x00, 0x00, 0x00, 0xDA, 0x80}, 5, 0}, \
        {0xB2, (uint8_t[]){0x3C, 0x02, 0x30}, 3, 0}, \
        {0xB3, (uint8_t[]){0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0}, \
        {0xB4, (uint8_t[]){0x80}, 1, 0}, \
        {0xB5, (uint8_t[]){0x0A, 0x0A}, 2, 0}, \
        {0xB6, (uint8_t[]){0x97, 0x97}, 2, 0}, \
        {0xB8, (uint8_t[]){0x26, 0x22, 0xF0, 0x63}, 4, 0}, \
        {0xBA, (uint8_t[]){0x31, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                           0x44, 0x25, 0x00, 0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0}, \
        {0xBC, (uint8_t[]){0x47}, 1, 0}, \
        {0xBF, (uint8_t[]){0x02, 0x11, 0x00}, 3, 0}, \
        {0xC0, (uint8_t[]){0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 9, 0}, \
        {0xC1, (uint8_t[]){0x43, 0x00, 0x32, 0x32, 0x77, 0xC1, 0xFF, 0xFF, 0xCC, 0xCC, 0x77, 0x77}, 12, 0}, \
        {0xC6, (uint8_t[]){0x82, 0x00, 0xBF, 0xFF, 0x00, 0xFF}, 6, 0}, \
        {0xC7, (uint8_t[]){0xB8, 0x00, 0x0A, 0x00, 0x00, 0x00}, 6, 0}, \
        {0xC8, (uint8_t[]){0x10, 0x40, 0x1E, 0x02}, 4, 0}, \
        {0xCC, (uint8_t[]){0x0B}, 1, 0}, \
        {0xE0, (uint8_t[]){0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D, 0x0F, 0x13, \
                           0x15, 0x13, 0x14, 0x0F, 0x16, 0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, \
                           0x3A, 0x07, 0x0D, 0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16}, 34, 0}, \
        {0xE3, (uint8_t[]){0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10}, 14, 0}, \
        {0xE9, (uint8_t[]){0xC8, 0x10, 0x0A, 0x00, 0x00, 0x80, 0x81, 0x12, 0x31, 0x23, 0x4F, 0x86, 0xA0, 0x00, 0x47, 0x08, \
                           0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x98, 0x02, 0x8B, 0xAF, \
                           0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x98, 0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, \
                           0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 63, 0}, \
        {0xEA, (uint8_t[]){0x97, 0x0C, 0x09, 0x09, 0x09, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x31, 0x8B, 0xA8, \
                           0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x9F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88, \
                           0x88, 0x88, 0x23, 0x00, 0x00, 0x02, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00, 0x00}, 61, 0}, \
        {0xEF, (uint8_t[]){0xFF, 0xFF, 0x01}, 3, 0}, \
        {0x11, (uint8_t[]){}, 0, 250}, \
        {0x29, (uint8_t[]){}, 0, 50}, \
    }

#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_LEFT,   .num = GPIO_NUM_13, .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_14, .pullup = 1, .level = 0},\
    {RG_KEY_UP,     .num = GPIO_NUM_15, .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_16, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_22, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_18, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_19, .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_20, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_21, .pullup = 1, .level = 0},\
    {RG_KEY_X,      .num = GPIO_NUM_12, .pullup = 1, .level = 0},\
    {RG_KEY_Y,      .num = GPIO_NUM_11, .pullup = 1, .level = 0},\
    {RG_KEY_L,      .num = GPIO_NUM_10, .pullup = 1, .level = 0},\
    {RG_KEY_R,      .num = GPIO_NUM_9,  .pullup = 1, .level = 0},\
}

#define RG_RECOVERY_BTN RG_KEY_MENU // Keep this button pressed to open the recovery menu

// MIPI-DSI Display (not SPI)
// GPIO_NUM_17 is needed for LCD_RST, RG_KEY_SELECT changed to GPIO_NUM_22
#define RG_GPIO_LCD_RST     GPIO_NUM_17  // Reset pin for ST7703
#define RG_GPIO_LCD_BCKL    -1           // Backlight not available on WHY2025

// SDMMC SD Card
// We use the default pins for SDMMC on the ESP32-P4 ie:
#define RG_GPIO_SDMMC_CLK    GPIO_NUM_43
#define RG_GPIO_SDMMC_CMD	 GPIO_NUM_44
#define RG_GPIO_SDMMC_D0	 GPIO_NUM_39
#define RG_GPIO_SDMMC_D1	 GPIO_NUM_40
#define RG_GPIO_SDMMC_D2	 GPIO_NUM_41
#define RG_GPIO_SDMMC_D3	 GPIO_NUM_42

// External I2S DAC
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_48
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_49
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_46
#define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_47
