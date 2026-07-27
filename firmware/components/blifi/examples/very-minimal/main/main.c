/**
 * @file main.c
 * @brief Very-minimal blifi example: the shortest possible integration.
 *
 * The entire device firmware is three calls in app_main(): build the default
 * config, blifi_init(), blifi_start(). No nvs_flash_init() - blifi_init()
 * initialises the default NVS partition itself (cfg.manage_nvs defaults to true;
 * the PoP is stored there). Set cfg.manage_nvs = false if your application
 * manages NVS on its own (see the "minimal" and "interactive" examples).
 *
 * On first boot the device advertises as "blifi-XXXX" and logs its
 * Proof-of-Possession; provision it with the companion app. Once provisioned it
 * reconnects to Wi-Fi on every boot.
 *
 * This is a getting-started sketch, not a production template: it lets blifi own
 * the default NVS partition and has no hardware factory-reset path. See the
 * README and the "minimal" / "interactive" examples before shipping.
 */
#include "esp_event.h"
#include "esp_log.h"

#include "blifi.h"

static const char *TAG = "very-minimal";

static void on_blifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const blifi_event_data_t *e = data;
    if (id == BLIFI_EVENT_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "online, ip=" IPSTR, IP2STR(&e->ip));
    } else {
        ESP_LOGI(TAG, "%s", blifi_status_str(e->status));
    }
}

void app_main(void)
{
    blifi_config_t cfg = BLIFI_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(blifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BLIFI_EVENT, ESP_EVENT_ANY_ID, on_blifi_event, NULL, NULL));
    ESP_ERROR_CHECK(blifi_start());

    ESP_LOGI(TAG, "provisioned: %s, PoP: %s",
             blifi_is_provisioned() ? "yes" : "no", blifi_get_pop());
}
