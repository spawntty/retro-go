#!/usr/bin/env python3
"""Compile production Wi-Fi lifecycle and patched Hosted functions with host mocks.

Run after configuring the WHY2025 launcher, so pinned managed sources exist.
These checks do not simulate SDIO timing or a radio.
"""
from pathlib import Path
import importlib.util
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / 'components/retro-go/targets/why2025/components/why2025_wifi'
HOSTED = ROOT / 'launcher/managed_components/espressif__esp_hosted'


def function(text, name):
    # These C functions have balanced braces, including their comments/strings.
    start = text.rfind('\n', 0, text.index(name + '(')) + 1
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def compile_run(directory, name, code):
    source = directory / (name + '.c')
    source.write_text(code)
    subprocess.run(['cc', '-std=c11', '-Werror=implicit-function-declaration',
                    '-I', str(ROOT / 'components/retro-go'), str(source),
                    '-o', str(directory / name)], check=True)
    subprocess.run([str(directory / name)], check=True)


NETWORK_MOCKS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define RG_EVENT_TYPE_NETWORK 0x200
#include "rg_network.h"
#define RG_ENABLE_NETWORKING
#define RG_TARGET_WHY2025
#define RG_TARGET_NAME "why2025"
#define RG_LOGE(...) ((void)0)
#define RG_LOGW(...) ((void)0)
#define RG_LOGI(...) ((void)0)
#define RG_LOGV(...) ((void)0)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NVS_NO_FREE_PAGES 3
#define ESP_ERR_NVS_NEW_VERSION_FOUND 4
#define WIFI_EVENT 1
#define IP_EVENT 2
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT_STA_STOP 1
#define WIFI_EVENT_AP_STOP 2
#define WIFI_EVENT_STA_START 3
#define WIFI_EVENT_STA_DISCONNECTED 4
#define WIFI_EVENT_AP_START 5
#define IP_EVENT_STA_GOT_IP 6
#define IP_EVENT_AP_STAIPASSIGNED 7
#define WIFI_STORAGE_RAM 0
#define SNTP_OPMODE_POLL 0
#define NS_WIFI "wifi"
#define WIFI_MODE_AP 1
#define WIFI_MODE_STA 2
#define WIFI_IF_AP 1
#define WIFI_IF_STA 2
#define WIFI_AUTH_WPA2_PSK 1
#define WIFI_AUTH_OPEN 0
#define WIFI_INIT_CONFIG_DEFAULT() {0}
typedef int esp_err_t;
typedef int esp_netif_t;
typedef int esp_event_base_t;
typedef struct {int ip;} ip_event_ap_staipassigned_t;
typedef int wifi_init_config_t;
typedef struct {
    struct {char ssid[32], password[64]; int channel, authmode, max_connection;} ap;
    struct {char ssid[32], password[64]; int channel;} sta;
} wifi_config_t;
static rg_network_state_t network_state;
static rg_wifi_config_t wifi_config;
static esp_netif_t *netif_sta, *netif_ap, *netif;
static bool wifi_initialized, wifi_started, event_loop_owned;
static int sta, ap, loops, interfaces, handlers, stops, deinits, erases, starts;
static int fail_init, fail_storage, fail_nvs, fail_start, shared_loop;
static int connections;
static int esp_wifi_connect(void) {++connections; return 0;}
static void rg_system_event(int event, void *arg) {}
rg_network_t rg_network_get_info(void) {return (rg_network_t){0};}
static int esp_event_loop_create_default(void) {
    if (shared_loop) return ESP_ERR_INVALID_STATE;
    ++loops; return ESP_OK;
}
static int esp_event_loop_delete_default(void) {--loops; return 0;}
static int esp_event_handler_register(int base, int id, void (*fn)(void *, esp_event_base_t, int32_t, void *), void *arg) {
    handlers |= base; return 0;
}
static int esp_event_handler_unregister(int base, int id, void (*fn)(void *, esp_event_base_t, int32_t, void *)) {
    handlers &= ~base; return 0;
}
static int esp_netif_init(void) {return 0;}
static esp_netif_t *esp_netif_create_default_wifi_sta(void) {++interfaces; return &sta;}
static esp_netif_t *esp_netif_create_default_wifi_ap(void) {++interfaces; return &ap;}
static void esp_netif_destroy_default_wifi(void *p) {assert(p); --interfaces;}
static int esp_netif_set_hostname(void *p, const char *name) {assert(p); return 0;}
static int nvs_flash_init(void) {return fail_nvs;}
static int nvs_flash_erase(void) {++erases; fail_nvs = 0; return 0;}
int why2025_wifi_prepare(void) {return 0;}
static int esp_wifi_init(const wifi_init_config_t *cfg) {return fail_init;}
static int esp_wifi_set_storage(int storage) {return fail_storage;}
static int esp_wifi_stop(void) {assert(!wifi_started); ++stops; return 0;}
static int esp_wifi_deinit(void) {++deinits; return 0;}
static int esp_wifi_set_mode(int mode) {return 0;}
static int esp_wifi_set_config(int iface, const wifi_config_t *cfg) {return 0;}
static int esp_wifi_start(void) {assert(wifi_started); ++starts; return fail_start;}
static void esp_sntp_stop(void) {}
static void esp_sntp_init(void) {}
static void esp_sntp_setoperatingmode(int mode) {}
static void esp_sntp_setservername(int n, const char *server) {}
static int rg_settings_get_number(const char *ns, const char *key, int def) {return def;}
static bool rg_settings_get_boolean(const char *ns, const char *key, bool def) {return def;}
bool rg_network_wifi_read_config(int slot, rg_wifi_config_t *out) {
    strcpy(out->ssid, "test"); return true;
}
'''

NETWORK_TESTS = r'''
static void clean_state(void) {
    assert(network_state == RG_NETWORK_DISABLED);
    assert(!wifi_initialized && !wifi_started);
    assert(!netif && !netif_sta && !netif_ap);
    assert(!interfaces && !handlers && !loops);
}
int main(void) {
    // Failed companion init must not issue further Wi-Fi RPCs or leak netifs.
    fail_init = ESP_FAIL;
    assert(!rg_network_init()); clean_state();
    assert(!stops && !deinits);
    assert(!rg_network_wifi_start()); rg_network_wifi_stop();
    assert(!stops && !starts);
    // Partial initialization must release the initialized driver.
    fail_init = 0; fail_storage = ESP_FAIL;
    assert(!rg_network_init()); clean_state();
    assert(stops == 1 && deinits == 1);
    // Unexpected NVS errors must not erase settings.
    fail_storage = 0; fail_nvs = ESP_FAIL;
    assert(!rg_network_init()); clean_state(); assert(!erases);
    // Known NVS exhaustion is recoverable; caller-owned event loop survives.
    fail_nvs = ESP_ERR_NVS_NO_FREE_PAGES; shared_loop = 1;
    assert(rg_network_init()); assert(erases == 1);
    assert(rg_network_init()); assert(interfaces == 2);
    assert(rg_network_wifi_start()); assert(wifi_started && netif == netif_sta);
    network_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_START, NULL);
    assert(connections == 1);
    network_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    assert(connections == 2 && network_state == RG_NETWORK_CONNECTING);
    rg_network_wifi_stop(); assert(!wifi_started && !netif);
    network_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    assert(connections == 2 && network_state == RG_NETWORK_DISCONNECTED);
    wifi_config.ap_mode = true;
    assert(rg_network_wifi_start()); assert(netif == netif_ap);
    rg_network_wifi_stop();
    fail_start = ESP_FAIL;
    assert(!rg_network_wifi_start()); assert(!wifi_started);
    rg_network_deinit(); clean_state();
    // A successful teardown can be followed by initialization again.
    shared_loop = 0; fail_start = 0;
    assert(rg_network_init()); assert(loops == 1);
    rg_network_deinit(); clean_state();
    rg_network_deinit(); clean_state();
}
'''

HOSTED_MOCKS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define TRANSPORT_RX_ACTIVE 1
#define SDIO_FAIL_IF_NULL(x) do {if (!(x)) return ESP_FAIL;} while (0)
#define HOSTED_FREE(x) do {free(x); ++freed;} while (0)
typedef int esp_err_t;
typedef int wifi_init_config_t;
typedef int sdmmc_card_t;
typedef struct {sdmmc_card_t *card; struct {int slot;} config;} sdmmc_context_t;
static bool ready;
static int transport_state, resets, sleeps, rpc_calls, slot_deinit = -1, freed;
static bool is_transport_tx_ready(void) {return ready;}
static bool esp_hosted_is_config_valid(void) {return true;}
static void reset_slave(void) {++resets;}
static void mock_sleep(int seconds) {sleeps += seconds; assert(sleeps <= 5);}
static struct {void (*_h_sleep)(int);} funcs = {mock_sleep};
static struct {__typeof__(funcs) *funcs;} g_h = {&funcs};
static int rpc_wifi_init(const wifi_init_config_t *cfg) {++rpc_calls; return ESP_OK;}
static void sdmmc_host_deinit(void) {assert(!"must not deinitialize the SD card slot");}
static void sdmmc_host_deinit_slot(int slot) {slot_deinit = slot;}
'''

HOSTED_TESTS = r'''
int main(void) {
    assert(esp_wifi_remote_init(NULL) == ESP_FAIL);
    assert(sleeps == 5 && resets == 1 && rpc_calls == 0);
    // The keyboard has now been configured: retries must not reset it again.
    assert(esp_wifi_remote_init(NULL) == ESP_FAIL);
    assert(sleeps == 5 && resets == 1 && rpc_calls == 0);
    ready = true;
    assert(esp_wifi_remote_init(NULL) == ESP_OK);
    assert(rpc_calls == 1 && resets == 1);
    ready = false;
    assert(esp_wifi_remote_init(NULL) == ESP_FAIL);
    assert(sleeps == 5 && resets == 1 && rpc_calls == 1);
    sdmmc_context_t ctx = {.card = malloc(sizeof(int)), .config = {.slot = 1}};
    assert(hosted_sdio_deinit(&ctx) == ESP_OK);
    assert(slot_deinit == 1 && freed == 1);
}
'''

BOARD_MOCKS = r'''
#include <assert.h>
#include <stdbool.h>
#define RG_TARGET_WHY2025
#define RG_ENABLE_NETWORKING
#define RG_LOGW(...) ((void)0)
#define ESP_OK 0
#define ESP_FAIL -1
#define GPIO_NUM_13 13
#define GPIO_MODE_OUTPUT 1
typedef int esp_err_t;
static bool storage_ready, keyboard_configured, boot_high, boot_output;
static int handshakes, companion_result;
static int gpio_set_level(int pin, int value) {
    assert(pin == GPIO_NUM_13 && value == 1);
    boot_high = true; return ESP_OK;
}
static int gpio_set_direction(int pin, int mode) {
    assert(pin == GPIO_NUM_13 && mode == GPIO_MODE_OUTPUT && boot_high);
    boot_output = true; return ESP_OK;
}
static int esp_hosted_connect_to_slave(void) {
    assert(storage_ready && boot_output);
    ++handshakes;
    keyboard_configured = false; // The actual shared GPIO12 reset clears TCA8418.
    return companion_result;
}
static void rg_storage_init(void) {storage_ready = true;}
static void rg_input_init(void) {assert(handshakes == 1); keyboard_configured = true;}
'''


def test_board_order(directory):
    system = (ROOT / 'components/retro-go/rg_system.c').read_text()
    start = system.index('    rg_storage_init();')
    end = system.index('    rg_input_init();', start) + len('    rg_input_init();')
    # Execute the production startup sequence, not a duplicate of its ordering.
    startup = system[start:end]
    prepare = function((COMPONENT / 'why2025_wifi.c').read_text(), 'why2025_wifi_prepare')
    for name, result in [('connected', 'ESP_OK'), ('missing', 'ESP_FAIL')]:
        code = BOARD_MOCKS + prepare + '\nint main(void) {\n'
        code += f'companion_result = {result};\n' + startup
        code += r'''
    assert(keyboard_configured && handshakes == 1);
    // Later network initialization must use the cached result, even on failure.
    assert(why2025_wifi_prepare() == companion_result);
    assert(why2025_wifi_prepare() == companion_result);
    assert(keyboard_configured && handshakes == 1);
}
'''
        compile_run(directory, 'board_' + name, code)


def main():
    if not HOSTED.is_dir():
        raise SystemExit('Configure/build the WHY2025 launcher first to fetch ESP-Hosted 2.0.17.')
    spec = importlib.util.spec_from_file_location('patch_hosted', COMPONENT / 'patch_hosted.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix='why2025-wifi-test-') as tmp:
        directory = Path(tmp)
        module.patch(HOSTED, directory)
        network = (ROOT / 'components/retro-go/rg_network.c').read_text()
        macros = network[network.index('#define TRY'):network.index('#ifdef RG_ENABLE_NETWORKING')]
        functions = ''.join(function(network, name) for name in (
            'network_event_handler', 'rg_network_wifi_start', 'rg_network_wifi_stop', 'rg_network_deinit', 'rg_network_init'))
        compile_run(directory, 'network', NETWORK_MOCKS + macros + functions + NETWORK_TESTS)
        transport = (directory / 'transport_drv.c').read_text()
        api = (directory / 'esp_hosted_api.c').read_text()
        sdio = (directory / 'sdio_wrapper.c').read_text()
        functions = function(transport, 'transport_drv_reconfigure')
        functions += function(api, 'esp_hosted_reconfigure')
        functions += function(api, 'esp_wifi_remote_init')
        functions += function(sdio, 'hosted_sdio_deinit')
        compile_run(directory, 'hosted', HOSTED_MOCKS + functions + HOSTED_TESTS)
        test_board_order(directory)
    print('Wi-Fi lifecycle, Hosted failure paths and shared keyboard reset checks passed.')


if __name__ == '__main__':
    main()
