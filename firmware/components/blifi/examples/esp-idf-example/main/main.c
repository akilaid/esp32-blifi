/**
 * @file main.c
 * @brief Serial-console demo for the blifi Wi-Fi manager (Phase 2, no BLE).
 *
 * Boots, auto-connects if credentials are stored in NVS, and exposes a REPL:
 *   scan | set <ssid> <pass> | connect [ssid pass] | status | erase | info
 */
#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "blifi.h"

static const char *TAG = "example";

static const char *authmode_str(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    case WIFI_AUTH_OWE:             return "OWE";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
    case WIFI_AUTH_WPA3_ENTERPRISE: return "WPA3-Ent";
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA2/3-Ent";
    default:                        return "?";
    }
}

/* Log every status transition the manager publishes. */
static void on_blifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const blifi_event_data_t *e = data;
    if (id == BLIFI_EVENT_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "[event] %s  ip=" IPSTR,
                 blifi_status_str(e->status), IP2STR(&e->ip));
    } else {
        ESP_LOGI(TAG, "[event] %s%s", blifi_status_str(e->status),
                 id == BLIFI_EVENT_REPROVISIONING_TRIGGERED ? "  (re-provisioning needed)" : "");
    }
}

/* ------------------------------------------------------------- REPL commands */

static int cmd_scan(int argc, char **argv)
{
    blifi_wifi_ap_t aps[20];
    size_t found = 0;
    esp_err_t err = blifi_wifi_manager_scan(aps, sizeof(aps) / sizeof(aps[0]), &found);
    if (err != ESP_OK) {
        printf("scan failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("%-32s %5s %5s %-10s\n", "SSID", "RSSI", "CH", "AUTH");
    for (size_t i = 0; i < found; i++) {
        printf("%-32s %5d %5u %-10s\n",
               aps[i].hidden ? "<hidden>" : aps[i].ssid,
               aps[i].rssi, aps[i].channel, authmode_str(aps[i].authmode));
    }
    printf("%u network(s)\n", (unsigned)found);
    return 0;
}

static int cmd_set(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: set <ssid> [password]\n");
        return 1;
    }
    blifi_wifi_credentials_t c = {0};
    strlcpy(c.ssid, argv[1], sizeof(c.ssid));
    if (argc >= 3) {
        strlcpy(c.password, argv[2], sizeof(c.password));
    }
    esp_err_t err = blifi_wifi_manager_save_credentials(&c);
    printf(err == ESP_OK ? "saved\n" : "save failed: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_connect(int argc, char **argv)
{
    esp_err_t err;
    if (argc >= 3) {
        blifi_wifi_credentials_t c = {0};
        strlcpy(c.ssid, argv[1], sizeof(c.ssid));
        strlcpy(c.password, argv[2], sizeof(c.password));
        err = blifi_wifi_manager_connect(&c);
    } else {
        err = blifi_wifi_manager_connect(NULL); /* stored credentials */
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("no stored credentials; use: connect <ssid> <pass>\n");
        return 1;
    }
    printf(err == ESP_OK ? "connecting...\n" : "connect failed: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_status(int argc, char **argv)
{
    blifi_status_t st = blifi_wifi_manager_status();
    printf("status: %s\n", blifi_status_str(st));
    esp_netif_ip_info_t ip;
    if (blifi_wifi_manager_get_ip(&ip) == ESP_OK) {
        printf("ip: " IPSTR "\n", IP2STR(&ip.ip));
    }
    return 0;
}

static int cmd_erase(int argc, char **argv)
{
    esp_err_t err = blifi_wifi_manager_erase_credentials();
    printf(err == ESP_OK ? "erased\n" : "erase failed: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_info(int argc, char **argv)
{
    blifi_wifi_credentials_t c;
    if (blifi_wifi_manager_load_credentials(&c) == ESP_OK) {
        printf("stored credentials: ssid=\"%s\"\n", c.ssid);
    } else {
        printf("no stored credentials\n");
    }
    return 0;
}

static void register_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "scan",    .help = "Scan for nearby Wi-Fi networks",           .func = &cmd_scan },
        { .command = "set",     .help = "Store credentials: set <ssid> [password]", .func = &cmd_set },
        { .command = "connect", .help = "Connect: connect [ssid password]",         .func = &cmd_connect },
        { .command = "status",  .help = "Show connection status and IP",            .func = &cmd_status },
        { .command = "erase",   .help = "Erase stored credentials",                 .func = &cmd_erase },
        { .command = "info",    .help = "Show stored credentials",                  .func = &cmd_info },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(blifi_wifi_manager_init(NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BLIFI_EVENT, ESP_EVENT_ANY_ID, on_blifi_event, NULL, NULL));

    /* Auto-connect from NVS to demonstrate persistence across reboots. */
    ESP_ERROR_CHECK(blifi_wifi_manager_start());

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "blifi>";
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    register_commands();

    printf("\nblifi Wi-Fi manager demo — type 'help'.\n");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
