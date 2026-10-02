/* TCA8418 keyboard matrix, using the register setup in BadgeVMS esp_tca8418.
 * Polled because the WHY2025 badge does not connect the interrupt pin. */
#include "rg_i2c.h"

#define TCA8418_INT_STAT 0x02
#define TCA8418_EVENT_COUNT 0x03
#define TCA8418_EVENT_FIFO 0x04
#define TCA8418_OVERFLOW 0x08
static uint8_t tca8418_keys[81];

static void tca8418_init(void)
{
    RG_ASSERT(rg_i2c_init(), "Keyboard I2C initialization failed");
    /* GPIO direction, event mask, interrupt level/enable, matrix, debounce.
     * All eight rows and ten columns are used, as in BadgeVMS. */
    static const uint8_t setup[][2] = {
        {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00},
        {0x20, 0xFF}, {0x21, 0xFF}, {0x22, 0xFF},
        {0x26, 0x00}, {0x27, 0x00}, {0x28, 0x00},
        {0x1A, 0xFF}, {0x1B, 0xFF}, {0x1C, 0xFF},
        {0x1D, 0xFF}, {0x1E, 0xFF}, {0x1F, 0x03},
        {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00},
    };
    for (size_t i = 0; i < RG_COUNT(setup); ++i)
        RG_ASSERT(rg_i2c_write_byte(RG_INPUT_TCA8418_ADDR, setup[i][0], setup[i][1]),
                  "Keyboard configuration failed");
    memset(tca8418_keys, 0, sizeof(tca8418_keys));
}

static const uint8_t *tca8418_read_keys(void)
{
    int status = rg_i2c_read_byte(RG_INPUT_TCA8418_ADDR, TCA8418_INT_STAT);
    int count = rg_i2c_read_byte(RG_INPUT_TCA8418_ADDR, TCA8418_EVENT_COUNT);
    if (status < 0 || count < 0)
    {
        memset(tca8418_keys, 0, sizeof(tca8418_keys));
        return tca8418_keys;
    }
    bool overflow = status & TCA8418_OVERFLOW;
    if (overflow)
        memset(tca8418_keys, 0, sizeof(tca8418_keys));
    /* Limit work per poll even if the bus/device misbehaves. On overflow,
     * discard the incomplete history so lost releases cannot stick keys. */
    count = RG_MIN(count & 0x0F, 10);
    for (int i = 0; i < count; ++i)
    {
        int event = rg_i2c_read_byte(RG_INPUT_TCA8418_ADDR, TCA8418_EVENT_FIFO);
        if (event < 0)
        {
            memset(tca8418_keys, 0, sizeof(tca8418_keys));
            return tca8418_keys;
        }
        unsigned key = event & 0x7F;
        if (!overflow && key > 0 && key < sizeof(tca8418_keys))
            tca8418_keys[key] = !!(event & 0x80);
    }
    if (!rg_i2c_write_byte(RG_INPUT_TCA8418_ADDR, TCA8418_INT_STAT, status & 0x0F))
        memset(tca8418_keys, 0, sizeof(tca8418_keys));
    return tca8418_keys;
}
