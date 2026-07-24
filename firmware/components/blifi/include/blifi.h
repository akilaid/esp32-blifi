/**
 * @file blifi.h
 * @brief Umbrella header for the blifi component and its esp_event event base.
 *
 * BLE-based Wi-Fi provisioning for ESP32. Phase 2 implements the Wi-Fi core
 * (scan/connect/persistence/retry) only; BLE transport, the security handshake,
 * and the provisioning state machine arrive in later phases. The `BLIFI_EVENT`
 * base and its full event list are declared here now so the API is stable; only
 * the Wi-Fi events are posted in Phase 2.
 */
#pragma once

#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include "blifi_status.h"
#include "blifi_wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** blifi event base for the default event loop. */
ESP_EVENT_DECLARE_BASE(BLIFI_EVENT);

/** Events posted on ::BLIFI_EVENT. Wi-Fi events are live in Phase 2. */
typedef enum {
    BLIFI_EVENT_STARTED = 0,          /*!< Component started (Phase 3) */
    BLIFI_EVENT_BLE_CONNECTED,        /*!< A central connected over BLE (Phase 3) */
    BLIFI_EVENT_CREDENTIALS_RECEIVED, /*!< Credentials received/accepted */
    BLIFI_EVENT_WIFI_CONNECTING,      /*!< Attempting to join the AP */
    BLIFI_EVENT_WIFI_CONNECTED,       /*!< Online; see ::blifi_event_data_t.ip */
    BLIFI_EVENT_WIFI_FAILED,          /*!< Gave up after retries; see .status */
    BLIFI_EVENT_PROV_TIMEOUT,         /*!< Provisioning window elapsed (Phase 3) */
    BLIFI_EVENT_REPROVISIONING_TRIGGERED, /*!< Repeated failure / manual trigger */
    BLIFI_EVENT_HARD_RESET_TRIGGERED, /*!< Boot after a reset-pin factory reset (§6.1) */
} blifi_event_id_t;

/** Data payload accompanying a ::BLIFI_EVENT post. */
typedef struct {
    blifi_status_t status;      /*!< Current status code */
    int            wifi_reason; /*!< Raw wifi_err_reason_t on failures, else 0 */
    esp_ip4_addr_t ip;          /*!< Valid on ::BLIFI_EVENT_WIFI_CONNECTED */
} blifi_event_data_t;

/** Top-level configuration. Prefer ::BLIFI_DEFAULT_CONFIG. */
typedef struct {
    const char *device_name;   /*!< BLE name; NULL → "blifi-XXXX" from the MAC */
    bool        require_pop;   /*!< Require Proof-of-Possession (default true) */
    uint32_t    prov_timeout_ms; /*!< Provisioning window; 0 = no timeout */
    blifi_wifi_manager_config_t wifi; /*!< Wi-Fi retry/backoff config */
} blifi_config_t;

#define BLIFI_DEFAULT_CONFIG() ((blifi_config_t){ \
    .device_name     = NULL,  \
    .require_pop     = true,  \
    .prov_timeout_ms = 0,     \
    .wifi            = BLIFI_WIFI_MANAGER_DEFAULT_CONFIG(), \
})

/**
 * @brief Initialise blifi: Wi-Fi manager, PoP (generated on first boot and
 *        logged once over UART), and the BLE stack. NVS must already be
 *        initialised by the application.
 */
esp_err_t blifi_init(const blifi_config_t *config);

/**
 * @brief Start operation: if credentials are stored, connect to Wi-Fi;
 *        otherwise advertise over BLE for provisioning.
 */
esp_err_t blifi_start(void);

/** @brief Stop Wi-Fi and BLE activity. */
esp_err_t blifi_stop(void);

/** @brief True if Wi-Fi credentials are stored. */
bool blifi_is_provisioned(void);

/** @brief Erase stored credentials and return to the provisioning state. */
esp_err_t blifi_reset_credentials(void);

/** @brief The device's Proof-of-Possession string (for display/logging). */
const char *blifi_get_pop(void);

/**
 * @brief Callback invoked once on the boot following a hard (reset-pin) reset,
 *        so the application can erase its own data (its NVS namespace(s),
 *        SPIFFS/LittleFS files, etc.). Wi-Fi credentials are already gone — the
 *        bootloader erased the dedicated `blifi_nvs` partition before app_main.
 * @param arg The opaque pointer passed to ::blifi_register_data_reset_callback.
 */
typedef void (*blifi_data_reset_cb_t)(void *arg);

/**
 * @brief Register an optional app-data reset callback for the hard-reset flow
 *        (§6.1). Call this **before** ::blifi_start (typically right before
 *        ::blifi_init); ::blifi_start fires it — and posts
 *        ::BLIFI_EVENT_HARD_RESET_TRIGGERED — when this boot follows a hard reset.
 *        Passing NULL clears any registration.
 */
esp_err_t blifi_register_data_reset_callback(blifi_data_reset_cb_t cb, void *arg);

/**
 * @brief True if this boot immediately follows a hard (reset-pin) factory reset.
 *        The underlying bootloader flag is consumed on first read during
 *        ::blifi_init, so this returns a stable cached value and reverts to false
 *        on the next normal boot.
 */
bool blifi_was_hard_reset(void);

#ifdef __cplusplus
}
#endif
