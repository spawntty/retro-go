#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_hosted.h"
#include <stdbool.h>

/* BadgeVMS's UART flasher identifies P4 GPIO12 as C6 EN and GPIO13 as BOOT.
 * Hosted pulses EN; keep BOOT high so the C6 starts its installed application.
 * GPIO12 also resets the TCA8418. Run the transport handshake BEFORE keyboard
 * initialization, and cache even failures so later Wi-Fi init cannot reset it.
 * No companion flash writes are needed for normal Wi-Fi operation.
 */
esp_err_t why2025_wifi_prepare(void)
{
    static bool attempted;
    static esp_err_t result = ESP_FAIL;
    if (attempted)
        return result;
    attempted = true;

    result = gpio_set_level(GPIO_NUM_13, 1);
    if (result != ESP_OK)
        return result;
    result = gpio_set_direction(GPIO_NUM_13, GPIO_MODE_OUTPUT);
    if (result != ESP_OK)
        return result;
    result = esp_hosted_connect_to_slave();
    return result;
}
