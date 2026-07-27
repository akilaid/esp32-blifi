/**
 * @file main.c
 * @brief Minimal blifi example: BLE Wi-Fi provisioning + reset pin + reset LED.
 *
 * The whole integration is ~20 lines in app_main(): init NVS, init blifi with
 * the default config, register the optional hard-reset hook, start. Everything
 * else - the reset *pin* (bootloader factory reset on GPIO13) and the reset
 * *LED* (GPIO2 pulses when credentials were wiped) - is configuration, not
 * code: see sdkconfig.defaults and partitions.csv.
 *
 * On first boot the device advertises as "blifi-XXXX" and logs its
 * Proof-of-Possession; provision it with the companion app. Once provisioned
 * it reconnects to Wi-Fi on every boot until the reset pin wipes the
 * credentials.
 */
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "blifi.h"

static const char *TAG = "minimal";

static void on_blifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const blifi_event_data_t *e = data;
    switch (id) {
    case BLIFI_EVENT_WIFI_CONNECTED:
        ESP_LOGI(TAG, "online, ip=" IPSTR, IP2STR(&e->ip));
        break;
    case BLIFI_EVENT_HARD_RESET_TRIGGERED:
        /* Boot follows a reset-pin hard reset: the bootloader already erased
         * the Wi-Fi credentials (blifi_nvs partition) and the indicator LED is
         * pulsing. Nothing to do here - see on_data_reset() for app data. */
        ESP_LOGW(TAG, "hard reset detected - back to provisioning mode");
        break;
    default:
        ESP_LOGI(TAG, "%s", blifi_status_str(e->status));
        break;
    }
}

/* Optional: called once on the boot after a reset-pin hard reset so the app can
 * erase its own data (own NVS namespaces, files, ...). Wi-Fi credentials are
 * already gone; the PoP survives so the printed QR/sticker keeps working. */
static void on_data_reset(void *arg)
{
    ESP_LOGW(TAG, "erasing app data after hard reset (nothing to erase in this demo)");
}

void app_main(void)
{
    /* This example manages the default NVS partition itself and tells blifi to
     * leave it alone (cfg.manage_nvs = false, below). For the shortest possible
     * integration - where blifi_init() handles NVS for you - see the
     * "very-minimal" example. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Defaults pick up the reset-indicator settings from Kconfig (LED on GPIO2,
     * 2 s pulse - see sdkconfig.defaults). Override at runtime here if needed:
     *   cfg.reset_indicator.gpio = 4;  cfg.reset_indicator.pulse_ms = 0;      */
    blifi_config_t cfg = BLIFI_DEFAULT_CONFIG();
    cfg.manage_nvs = false; /* the app initialised NVS above */

    blifi_register_data_reset_callback(on_data_reset, NULL); /* before init */
    ESP_ERROR_CHECK(blifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BLIFI_EVENT, ESP_EVENT_ANY_ID, on_blifi_event, NULL, NULL));
    ESP_ERROR_CHECK(blifi_start());

    ESP_LOGI(TAG, "provisioned: %s, PoP: %s",
             blifi_is_provisioned() ? "yes" : "no", blifi_get_pop());
}
