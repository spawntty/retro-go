/* Configuration for Retro Ruler second revision, based on the ESP32-S3.
*
* Hardware info: https://github.com/rapha-tech/Retro-Ruler
*
*/

// Target definition
#define RG_TARGET_NAME             "RETRO-RULER"

// Storage
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDMMC_HOST       1
#define RG_STORAGE_SDMMC_SPEED      SDMMC_FREQ_DEFAULT

// GPIO Extender
// #define RG_I2C_GPIO_DRIVER          0   // 1 = AW9523, 2 = PCF9539, 3 = MCP23017
#define RG_I2C_GPIO_ADDR            0x20

// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video
<<<<<<< Updated upstream
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI2_HOST
=======
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341
#define RG_SCREEN_HOST              SPI3_HOST
>>>>>>> Stashed changes
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_80M
#define RG_SCREEN_BACKLIGHT         0
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
<<<<<<< Updated upstream
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                   \
    ILI9341_CMD(0xC5, 0x1A);                         /* VCOM */                                            \
    ILI9341_CMD(0x36, 0x60);                         /* Display Rotation */                                \
    ILI9341_CMD(0xB2, 0x05, 0x05, 0x00, 0x33, 0x33); /* Porch Setting */                                   \
    ILI9341_CMD(0xB7, 0x05);                         /* Gate Control //12.2v   -10.43v */                  \
    ILI9341_CMD(0xBB, 0x3F);                         /* VCOM */                                            \
    ILI9341_CMD(0xC0, 0x2c);                         /* Power control */                                   \
    ILI9341_CMD(0xC2, 0x01);                         /* VDV and VRH Command Enable */                      \
    ILI9341_CMD(0xC3, 0x0F);                         /* VRH Set 4.3+( vcom+vcom offset+vdv) */             \
    ILI9341_CMD(0xC4, 0xBE);                         /* VDV Set 0v */                                      \
    ILI9341_CMD(0xC6, 0X01);                         /* Frame Rate Control in Normal Mode 111Hz */         \
    ILI9341_CMD(0xD0, 0xA4, 0xA1);                   /* Power Control 1 */                                 \
    ILI9341_CMD(0xE8, 0x03);                         /* Power Control 1 */                                 \
    ILI9341_CMD(0xE9, 0x09, 0x09, 0x08);             /* Equalize time control */                           \
    ILI9341_CMD(0xE0, 0xD0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3F, 0x07, 0x13, 0x14, 0x28, 0x30); \
    ILI9341_CMD(0xE1, 0xD0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F, 0x1F);

// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types
#define RG_GAMEPAD_I2C_MAP {\
    {RG_KEY_UP,     .num = 2, .level = 0},\
    {RG_KEY_RIGHT,  .num = 5, .level = 0},\
    {RG_KEY_DOWN,   .num = 3, .level = 0},\
    {RG_KEY_LEFT,   .num = 4, .level = 0},\
    {RG_KEY_SELECT, .num = 1, .level = 0},\
    {RG_KEY_START,  .num = 0, .level = 0},\
    {RG_KEY_A,      .num = 6, .level = 0},\
    {RG_KEY_B,      .num = 7, .level = 0},\
}
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_L,      .num = GPIO_NUM_40, .pullup = 1, .level = 0},\
    {RG_KEY_R,      .num = GPIO_NUM_41, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_42, .pullup = 1, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_41, .pullup = 1, .level = 0},\
}

// Battery
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_3
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3500.f) / (4200.f - 3500.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)


// Status LED
#define RG_GPIO_LED                 GPIO_NUM_2

// I2C BUS
#define RG_GPIO_I2C_SDA             GPIO_NUM_10
#define RG_GPIO_I2C_SCL             GPIO_NUM_11
=======
#define RG_SCREEN_MARGIN_TOP        0
#define RG_SCREEN_MARGIN_BOTTOM     0
#define RG_SCREEN_MARGIN_LEFT       0
#define RG_SCREEN_MARGIN_RIGHT      0
#define RG_SCREEN_INIT()                                                                                     \
ILI9341_CMD(0xCF, 0x00, 0xc3, 0x30);                                                                         \
ILI9341_CMD(0xED, 0x64, 0x03, 0x12, 0x81);                                                                   \
ILI9341_CMD(0xE8, 0x85, 0x00, 0x78);                                                                         \
ILI9341_CMD(0xCB, 0x39, 0x2c, 0x00, 0x34, 0x02);                                                             \
ILI9341_CMD(0xF7, 0x20);                                                                                     \
ILI9341_CMD(0xEA, 0x00, 0x00);                                                                               \
ILI9341_CMD(0xC0, 0x1B);                 /* Power control   //VRH[5:0] */                                    \
ILI9341_CMD(0xC1, 0x12);                 /* Power control   //SAP[2:0];BT[3:0] */                            \
ILI9341_CMD(0xC5, 0x32, 0x3C);           /* VCM control */                                                   \
ILI9341_CMD(0xC7, 0x91);                 /* VCM control2 */                                                  \
ILI9341_CMD(0x36, 0x08); /* Memory Access Control */                                         \
ILI9341_CMD(0xB1, 0x00, 0x10);           /* Frame Rate Control (1B=70, 1F=61, 10=119) */                     \
ILI9341_CMD(0xB6, 0x0A, 0xA2);           /* Display Function Control */                                      \
ILI9341_CMD(0xF6, 0x01, 0x30);                                                                               \
ILI9341_CMD(0xF2, 0x00); /* 3Gamma Function Disable */                                                       \
ILI9341_CMD(0x26, 0x01); /* Gamma curve selected */                                                          \
ILI9341_CMD(0xE0, 0xD0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0e, 0x12, 0x14, 0x17);       \
ILI9341_CMD(0xE1, 0xD0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1b, 0x1e);       

#define RG_GAMEPAD_GPIO_MAP {\
{RG_KEY_LEFT,   GPIO_NUM_13,  GPIO_PULLUP_ONLY, 0},\
{RG_KEY_RIGHT,  GPIO_NUM_14, GPIO_PULLUP_ONLY, 0},\
{RG_KEY_UP,     GPIO_NUM_15,  GPIO_PULLUP_ONLY, 0},\
{RG_KEY_DOWN,   GPIO_NUM_16, GPIO_PULLUP_ONLY, 0},\
{RG_KEY_SELECT, GPIO_NUM_17,  GPIO_PULLUP_ONLY, 0},\
{RG_KEY_START,  GPIO_NUM_18,  GPIO_PULLUP_ONLY, 0},\
{RG_KEY_MENU,   GPIO_NUM_19, GPIO_PULLUP_ONLY, 0},\
{RG_KEY_A,      GPIO_NUM_20,  GPIO_PULLUP_ONLY, 0},\
{RG_KEY_B,      GPIO_NUM_21,  GPIO_PULLUP_ONLY, 0},\
}

#define RG_RECOVERY_BTN RG_KEY_MENU // Keep this button pressed to open the recovery menu
>>>>>>> Stashed changes

// SPI Display
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_22
#define RG_GPIO_LCD_CLK             GPIO_NUM_23
#define RG_GPIO_LCD_CS              GPIO_NUM_24
#define RG_GPIO_LCD_DC              GPIO_NUM_25
#define RG_GPIO_LCD_RST             GPIO_NUM_26
#define RG_GPIO_LCD_BCKL            GPIO_NUM_27

// SPI SD Card
/*
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_43
#define RG_GPIO_SDSPI_CMD           GPIO_NUM_44
#define RG_GPIO_SDSPI_D0            GPIO_NUM_39
*/

// External I2S DAC
<<<<<<< Updated upstream
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_38
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_13
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_9
#define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_18
=======
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_28
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_29
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_30
#define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_31

#define RG_ZIP_SUPPORT 0
>>>>>>> Stashed changes
