/* WHY2025 badge. Pinout and panel configuration follow BadgeVMS. */
#define RG_TARGET_NAME              "why2025"

#define RG_GPIO_I2C_SDA              GPIO_NUM_18
#define RG_GPIO_I2C_SCL              GPIO_NUM_20
#define RG_INPUT_TCA8418_ADDR        0x34

#define RG_STORAGE_ROOT              "/sd"
#define RG_STORAGE_SDMMC_HOST        SDMMC_HOST_SLOT_0
#define RG_STORAGE_SDMMC_WIDTH       4
#define RG_STORAGE_SDMMC_SPEED       SDMMC_FREQ_HIGHSPEED
#define RG_GPIO_SDMMC_CLK            GPIO_NUM_43
#define RG_GPIO_SDMMC_CMD            GPIO_NUM_44
#define RG_GPIO_SDMMC_D0             GPIO_NUM_39
#define RG_GPIO_SDMMC_D1             GPIO_NUM_40
#define RG_GPIO_SDMMC_D2             GPIO_NUM_41
#define RG_GPIO_SDMMC_D3             GPIO_NUM_42

/* MAX98357A on the carrier; trace through M.2 pins, not the MCU net names. */
#define RG_AUDIO_USE_INT_DAC         0
#define RG_AUDIO_USE_EXT_DAC         1
#define RG_AUDIO_EXT_DAC_MONO        1
#define RG_AUDIO_I2S_SAMPLE_RATE     48000 // MAX98357A does not support every emulator's native rate
#define RG_GPIO_SND_I2S_BCK          GPIO_NUM_26
#define RG_GPIO_SND_I2S_WS           GPIO_NUM_25
#define RG_GPIO_SND_I2S_DATA         GPIO_NUM_27
#define RG_GPIO_SND_AMP_ENABLE       GPIO_NUM_24
/* Battery measurement is not implemented. */
#define RG_BATTERY_DRIVER            0

#define RG_SCREEN_DRIVER            3   // ST7703 over MIPI-DSI
#define RG_SCREEN_WIDTH             720
#define RG_SCREEN_HEIGHT            720
#define RG_SCREEN_ROTATION          90  // ST7703: degrees CCW (0 or 90)
#define RG_SCREEN_RGB_BGR           1   // BadgeVMS uses BGR for this panel
#define RG_SCREEN_PIXEL_FORMAT      1   // Native little-endian RGB565
#define RG_SCREEN_BACKLIGHT         0   // No backlight GPIO in BadgeVMS
#define RG_SCREEN_PARTIAL_UPDATES    1
#define RG_GPIO_LCD_RST             GPIO_NUM_17
/* Default matches BadgeVMS v2.1: Bono (black border). Use 1 for Mountain (blue). */
#ifndef RG_WHY2025_PANEL_MOUNTAIN
#define RG_WHY2025_PANEL_MOUNTAIN    0
#endif

/* TCA8418 FIFO scancodes from BadgeVMS drivers/tca8418.c. */
#define RG_GAMEPAD_KBD_MAP { \
    {RG_KEY_UP,     .src = 0x17}, /* W */ \
    {RG_KEY_DOWN,   .src = 0x21}, /* S */ \
    {RG_KEY_LEFT,   .src = 0x20}, /* A */ \
    {RG_KEY_RIGHT,  .src = 0x22}, /* D */ \
    {RG_KEY_A,      .src = 0x05}, /* Circle */ \
    {RG_KEY_B,      .src = 0x04}, /* Cross */ \
    {RG_KEY_X,      .src = 0x03}, /* Triangle */ \
    {RG_KEY_Y,      .src = 0x02}, /* Square */ \
    {RG_KEY_L,      .src = 0x06}, /* Cloud */ \
    {RG_KEY_R,      .src = 0x07}, /* Diamond */ \
    {RG_KEY_START,  .src = 0x3B}, /* Return */ \
    {RG_KEY_SELECT, .src = 0x08}, /* Backspace */ \
    {RG_KEY_MENU,   .src = 0x01}, /* Escape */ \
    {RG_KEY_OPTION, .src = 0x15}, /* Tab */ \
}
#define RG_RECOVERY_BTN             RG_KEY_MENU
#define RG_UPDATER_ENABLE           0
