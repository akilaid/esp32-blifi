/**
 * @file main.c
 * @brief blifi provisioning demo: BLE provisioning + Wi-Fi.
 *
 * On boot: initialise blifi, log the Proof-of-Possession, and start - connecting
 * to Wi-Fi if provisioned, otherwise advertising over BLE. REPL commands:
 *   status | pop | reset | selftest
 */
#include <stdio.h>

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "blifi.h"
#include "blifi_selftest.h"

static const char *TAG = "example";

static void on_blifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const blifi_event_data_t *e = data;
    if (id == BLIFI_EVENT_HARD_RESET_TRIGGERED) {
        ESP_LOGW(TAG, "[event] HARD_RESET_TRIGGERED - Wi-Fi credentials cleared by reset pin");
    } else if (id == BLIFI_EVENT_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "[event] %s  ip=" IPSTR, blifi_status_str(e->status), IP2STR(&e->ip));
    } else {
        ESP_LOGI(TAG, "[event] %s", blifi_status_str(e->status));
    }
}

/* Opt-in hook: on the boot after a reset-pin hard reset, blifi calls this so the
 * app can wipe its own data. Wi-Fi credentials are already gone; the PoP (in the
 * default nvs partition) is preserved. */
static void on_data_reset(void *arg)
{
    ESP_LOGW(TAG, "[hard-reset] app would erase its own data here (demo: nothing to erase)");
}

static int cmd_status(int argc, char **argv)
{
    printf("provisioned: %s\n", blifi_is_provisioned() ? "yes" : "no");
    printf("wifi status: %s\n", blifi_status_str(blifi_wifi_manager_status()));
    esp_netif_ip_info_t ip;
    if (blifi_wifi_manager_get_ip(&ip) == ESP_OK) {
        printf("ip: " IPSTR "\n", IP2STR(&ip.ip));
    }
    return 0;
}

static int cmd_pop(int argc, char **argv)
{
    printf("Proof-of-Possession: %s\n", blifi_get_pop());
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    esp_err_t err = blifi_reset_credentials();
    printf(err == ESP_OK ? "reset - back to provisioning\n" : "reset failed: %s\n",
           esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_selftest(int argc, char **argv)
{
    int fails = blifi_selftest();
    printf(fails == 0 ? "self-test: ALL PASSED\n" : "self-test: %d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}

static int cmd_hardreset(int argc, char **argv)
{
    printf("was hard reset this boot: %s\n", blifi_was_hard_reset() ? "yes" : "no");
    return 0;
}

static void register_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "status",  .help = "Show provisioning + Wi-Fi status", .func = &cmd_status },
        { .command = "pop",     .help = "Show the Proof-of-Possession",     .func = &cmd_pop },
        { .command = "reset",   .help = "Erase credentials, re-provision",  .func = &cmd_reset },
        { .command = "selftest",.help = "Run crypto/framing self-tests",    .func = &cmd_selftest },
        { .command = "hardreset?",.help = "Report if this boot followed a reset-pin hard reset", .func = &cmd_hardreset },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
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

    blifi_config_t cfg = BLIFI_DEFAULT_CONFIG();
    cfg.manage_nvs = false; /* the app initialised NVS above */
    blifi_register_data_reset_callback(on_data_reset, NULL); /* before init */
    ESP_ERROR_CHECK(blifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BLIFI_EVENT, ESP_EVENT_ANY_ID, on_blifi_event, NULL, NULL));
    ESP_ERROR_CHECK(blifi_start());

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "blifi>";
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    register_commands();

    printf("\nblifi provisioning demo - type 'help'.\n");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
