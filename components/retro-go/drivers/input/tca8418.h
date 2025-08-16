#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "rg_input.h"
#include "rg_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "esp_tca8418.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCA8418_MAX_ROW_COUNT 8
#define TCA8418_MAX_COLUMN_COUNT 10
#define TCA8418_I2C_DEFAULT_ADDRESS 0x34

typedef struct {
    tca8418_dev_t *esp_device;
} rg_tca8418_t;

typedef struct {
    uint8_t scancode;
    bool pressed;
    rg_key_t mapped_key;
} rg_tca8418_event_t;

typedef struct {
    uint8_t scancode;
    rg_key_t key;
} rg_keymap_tca8418_t;

// Function declarations
rg_tca8418_t *rg_tca8418_init(gpio_num_t scl_pin, gpio_num_t sda_pin, uint8_t i2c_address, gpio_num_t notify_pin, uint8_t rows, uint8_t cols);
void rg_tca8418_deinit(rg_tca8418_t *device);
bool rg_tca8418_read_events(rg_tca8418_t *device, rg_tca8418_event_t *events, size_t max_events, size_t *event_count);
uint8_t rg_tca8418_get_event_count(rg_tca8418_t *device);
void rg_tca8418_flush(rg_tca8418_t *device);

// Implementation (inline for header-only style like display drivers)
#define TAG "TCA8418"

#ifdef RG_GAMEPAD_TCA8418_MAP
static rg_keymap_tca8418_t keymap_tca8418[] = RG_GAMEPAD_TCA8418_MAP;
#endif

static rg_key_t rg_tca8418_scancode_to_key(uint8_t scancode)
{
#ifdef RG_GAMEPAD_TCA8418_MAP
    for (size_t i = 0; i < RG_COUNT(keymap_tca8418); i++) {
        if (keymap_tca8418[i].scancode == scancode) {
            return keymap_tca8418[i].key;
        }
    }
#endif
    return RG_KEY_NONE;
}

rg_tca8418_t *rg_tca8418_init(gpio_num_t scl_pin, gpio_num_t sda_pin, uint8_t i2c_address, gpio_num_t notify_pin, uint8_t rows, uint8_t cols)
{
    rg_tca8418_t *device = calloc(1, sizeof(rg_tca8418_t));
    if (!device) {
        RG_LOGE("Failed to allocate TCA8418 device");
        return NULL;
    }

    // Use the esp_tca8418 component to create the device
    device->esp_device = tca8418_create(
        scl_pin,
        sda_pin,
        i2c_address ? i2c_address : TCA8418_I2C_DEFAULT_ADDRESS,
        notify_pin,
        rows ? rows : TCA8418_MAX_ROW_COUNT,
        cols ? cols : TCA8418_MAX_COLUMN_COUNT
    );

    if (!device->esp_device) {
        RG_LOGE("Failed to initialize TCA8418 using esp_tca8418 component");
        free(device);
        return NULL;
    }

    // Flush any pending events
    rg_tca8418_flush(device);

    RG_LOGI("TCA8418 initialized successfully using esp_tca8418 component");
    return device;
}

void rg_tca8418_deinit(rg_tca8418_t *device)
{
    if (device) {
        if (device->esp_device) {
            tca8418_delete(device->esp_device);
        }
        free(device);
    }
}

bool rg_tca8418_read_events(rg_tca8418_t *device, rg_tca8418_event_t *events, size_t max_events, size_t *event_count)
{
    if (!device || !device->esp_device || !events || !event_count) {
        return false;
    }

    *event_count = 0;
    uint8_t count = tca8418_get_event_count(device->esp_device);
    
    for (uint8_t i = 0; i < count && *event_count < max_events; i++) {
        uint8_t raw_key = tca8418_get_key(device->esp_device);
        if (raw_key != 0) {
            events[*event_count].scancode = raw_key & 0x7F;
            events[*event_count].pressed = (raw_key & 0x80) != 0;
            events[*event_count].mapped_key = rg_tca8418_scancode_to_key(events[*event_count].scancode);
            
            RG_LOGI("Key %s: scancode=0x%02X, mapped_key=%d", 
                    events[*event_count].pressed ? "pressed" : "released",
                    events[*event_count].scancode,
                    events[*event_count].mapped_key);
            
            (*event_count)++;
        }
    }

    return *event_count > 0;
}

uint8_t rg_tca8418_get_event_count(rg_tca8418_t *device)
{
    if (device && device->esp_device) {
        return tca8418_get_event_count(device->esp_device);
    }
    return 0;
}

void rg_tca8418_flush(rg_tca8418_t *device)
{
    if (device && device->esp_device) {
        tca8418_flush(device->esp_device);
    }
}


#ifdef __cplusplus
}
#endif