"""WHY2025 compatibility fixes for ESP-Hosted 2.0.17; generated files only."""
from pathlib import Path
import sys


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"ESP-Hosted 2.0.17 source mismatch: {old!r}")
    return text.replace(old, new, 1)


def patch(source, output):
    changes = {
        "host/api/src/esp_hosted_api.c": [
            ("ESP_ERROR_CHECK(transport_drv_reconfigure());",
             "esp_err_t err = transport_drv_reconfigure();\n\tif (err != ESP_OK) return err;"),
            ("ESP_ERROR_CHECK(esp_hosted_reconfigure());",
             "esp_err_t err = esp_hosted_reconfigure();\n\tif (err != ESP_OK) return err;"),
        ],
        "host/drivers/transport/transport_drv.c": [
            # GPIO12 resets the keyboard too. Only early board initialization
            # may reset the companion; a later lost link requires a reboot.
            ("static int retry_slave_connection = 0;",
             "static int retry_slave_connection = 0;\n\tstatic bool reset_attempted = false;"),
            ("if (!is_transport_tx_ready()) {\n\t\treset_slave();",
             "if (!is_transport_tx_ready()) {\n\t\tif (reset_attempted) return ESP_FAIL;\n\t\treset_attempted = true;\n\t\treset_slave();"),
            ("retry_slave_connection < MAX_RETRY_TRANSPORT_ACTIVE",
             "retry_slave_connection < 5"),
        ],
        "host/port/esp/freertos/src/sdio_wrapper.c": [
            # Keep the allocated card/slot until explicit transport teardown.
            # In particular, do not disable slot 0 (the mounted SD card).
            ("fail:\n\tsdmmc_host_deinit();\n\tif (context->card) {\n\t\tHOSTED_FREE(context->card);\n\t}\n\treturn ESP_FAIL;",
             "fail:\n\treturn ESP_FAIL;"),
            ("\t\tsdmmc_host_deinit();", "\t\tsdmmc_host_deinit_slot(context->config.slot);"),
        ],
        "host/drivers/transport/sdio/sdio_drv.c": [
            # A FreeRTOS task cannot return. Suspend it on failed enumeration;
            # its handle stays valid for the upstream teardown code.
            ('ESP_LOGE(TAG, "sdio card init failed");\n\t\treturn;',
             'ESP_LOGE(TAG, "sdio card init failed; reboot to retry Wi-Fi");\n\t\tvTaskSuspend(NULL);\n\t\treturn;'),
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    for relative, replacements in changes.items():
        original = source / relative
        text = original.read_text()
        for old, new in replacements:
            text = replace_once(text, old, new)
        destination = output / original.name
        if not destination.exists() or destination.read_text() != text:
            destination.write_text(text)


if __name__ == "__main__":
    patch(Path(sys.argv[1]), Path(sys.argv[2]))
