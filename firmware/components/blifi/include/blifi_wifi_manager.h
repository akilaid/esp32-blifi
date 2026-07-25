/**
 * @file blifi_wifi_manager.h
 * @brief Wi-Fi station manager: scan, connect, NVS credential persistence, and
 *        a retry/backoff connection state machine.
 *
 * This is the Phase 2 surface of the blifi component - usable stand-alone (no
 * BLE). Status changes are reported as ::BLIFI_EVENT posts on the default event
 * loop (see blifi.h). Later phases layer the provisioning manager on top and
 * drive this module from received credentials.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "blifi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One scan result. Mirrors docs/protocol-spec.md §6.3. */
typedef struct {
    char            ssid[33];  /*!< NUL-terminated SSID (max 32 bytes) */
    uint8_t         bssid[6];  /*!< AP MAC */
    int8_t          rssi;      /*!< Signal strength, dBm */
    wifi_auth_mode_t authmode; /*!< Auth mode (WPA2_PSK, etc.) */
    uint8_t         channel;   /*!< Primary channel */
    bool            hidden;    /*!< Empty/hidden SSID */
} blifi_wifi_ap_t;

/** Wi-Fi credentials. `bssid`/`channel` are optional connect hints. */
typedef struct {
    char    ssid[33];
    char    password[64];
    uint8_t bssid[6];
    uint8_t channel;
    bool    bssid_set; /*!< true if `bssid`/`channel` should be used */
} blifi_wifi_credentials_t;

/** Manager configuration. Prefer ::BLIFI_WIFI_MANAGER_DEFAULT_CONFIG. */
typedef struct {
    uint8_t  max_retries;              /*!< Consecutive failures before FAILED */
    uint32_t backoff_base_ms;          /*!< First reconnect delay */
    uint32_t backoff_max_ms;           /*!< Cap on reconnect delay */
    uint32_t backoff_multiplier;       /*!< Delay growth factor per attempt */
    uint32_t connect_timeout_ms;       /*!< Per-attempt association timeout */
    bool     fast_fail_on_auth_error;  /*!< Give up immediately on wrong password */
    const char *nvs_partition;         /*!< NVS partition for credentials; NULL = default "nvs" */
} blifi_wifi_manager_config_t;

/** Sensible defaults: 5 retries, 1s→30s backoff (×2), 15s timeout, fast-fail auth. */
#define BLIFI_WIFI_MANAGER_DEFAULT_CONFIG() ((blifi_wifi_manager_config_t){ \
    .max_retries             = 5,     \
    .backoff_base_ms         = 1000,  \
    .backoff_max_ms          = 30000, \
    .backoff_multiplier      = 2,     \
    .connect_timeout_ms      = 15000, \
    .fast_fail_on_auth_error = true,  \
    .nvs_partition           = NULL,  \
})

/**
 * @brief Initialise the Wi-Fi station manager.
 *
 * Idempotently brings up esp_netif, the default event loop, a default STA netif,
 * and the Wi-Fi driver in STA mode, then registers Wi-Fi/IP handlers. Credentials
 * live in the `"blifi"` NVS namespace within `config->nvs_partition` (or the
 * default `nvs` partition when NULL); that partition must already be initialised.
 *
 * @param config Configuration, or NULL for ::BLIFI_WIFI_MANAGER_DEFAULT_CONFIG.
 * @return ESP_OK on success, or an esp_err_t from the underlying init calls.
 */
esp_err_t blifi_wifi_manager_init(const blifi_wifi_manager_config_t *config);

/**
 * @brief Blocking active scan for nearby access points.
 * @param out    Caller buffer for results (may be NULL if `max` is 0).
 * @param max    Capacity of `out`.
 * @param[out] found Number of APs written to `out` (and total seen, capped at max).
 * @return ESP_OK on success.
 */
esp_err_t blifi_wifi_manager_scan(blifi_wifi_ap_t *out, size_t max, size_t *found);

/** @name Credential persistence (NVS namespace "blifi") */
/**@{*/
esp_err_t blifi_wifi_manager_save_credentials(const blifi_wifi_credentials_t *creds);
/** @return ESP_OK, or ESP_ERR_NVS_NOT_FOUND if no credentials are stored. */
esp_err_t blifi_wifi_manager_load_credentials(blifi_wifi_credentials_t *out);
bool      blifi_wifi_manager_has_credentials(void);
esp_err_t blifi_wifi_manager_erase_credentials(void);
/**@}*/

/**
 * @brief Connect using the given credentials (persisted to NVS), starting the
 *        retry/backoff state machine. Pass NULL to connect with stored creds.
 * @return ESP_OK if the attempt started; ESP_ERR_NVS_NOT_FOUND if NULL and none stored.
 */
esp_err_t blifi_wifi_manager_connect(const blifi_wifi_credentials_t *creds);

/** @brief Auto-connect if credentials are stored; no-op otherwise. */
esp_err_t blifi_wifi_manager_start(void);

/** @brief Disconnect and stop retrying (returns to IDLE). */
esp_err_t blifi_wifi_manager_stop(void);

/** @brief Current status. */
blifi_status_t blifi_wifi_manager_status(void);

/** @brief Copy current IPv4 info; ESP_ERR_INVALID_STATE if not connected. */
esp_err_t blifi_wifi_manager_get_ip(esp_netif_ip_info_t *out);

#ifdef __cplusplus
}
#endif
