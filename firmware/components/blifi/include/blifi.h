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
    BLIFI_EVENT_STARTED = 0,          /*!< blifi_start() completed */
    BLIFI_EVENT_BLE_CONNECTED,        /*!< A central connected over BLE */
    BLIFI_EVENT_CREDENTIALS_RECEIVED, /*!< Credentials received/accepted */
    BLIFI_EVENT_WIFI_CONNECTING,      /*!< Attempting to join the AP */
    BLIFI_EVENT_WIFI_CONNECTED,       /*!< Online; see ::blifi_event_data_t.ip */
    BLIFI_EVENT_WIFI_FAILED,          /*!< Gave up after retries; back to provisioning. See .status */
    BLIFI_EVENT_PROV_TIMEOUT,         /*!< Provisioning window elapsed (::blifi_config_t.prov_timeout_ms) */
    BLIFI_EVENT_HARD_RESET_TRIGGERED, /*!< Boot after a reset-pin factory reset (§6.1) */
} blifi_event_id_t;

/** Data payload accompanying a ::BLIFI_EVENT post. */
typedef struct {
    blifi_status_t status;      /*!< Current status code */
    int            wifi_reason; /*!< Raw wifi_err_reason_t on failures, else 0 */
    esp_ip4_addr_t ip;          /*!< Valid on ::BLIFI_EVENT_WIFI_CONNECTED */
} blifi_event_data_t;

/**
 * Hard-reset indicator pin (optional, docs/plan.md §6.2). Only takes effect when
 * the component is built with `CONFIG_BLIFI_RESET_INDICATOR_ENABLE=y`; otherwise
 * the whole feature is compiled out and these fields are ignored. Defaults come
 * from the `Hard Reset Indicator` Kconfig menu; override at runtime here (e.g. via
 * the Arduino `Blifi.begin(config)`).
 */
typedef struct {
    bool     enable;       /*!< Runtime on/off (within a compiled-in build) */
    int8_t   gpio;         /*!< Output pin driven on hard-reset detection */
    uint8_t  active_level; /*!< Asserted level: 1 = high, 0 = low */
    uint32_t pulse_ms;     /*!< Assert duration; 0 = hold until re-provisioned */
} blifi_reset_indicator_config_t;

/* Kconfig-derived defaults for ::blifi_reset_indicator_config_t, with fallbacks so
 * this header compiles when the (disabled-by-default) sub-options are absent. */
#ifdef CONFIG_BLIFI_RESET_INDICATOR_ENABLE
#define BLIFI_RI_ENABLE_DEFAULT   true
#else
#define BLIFI_RI_ENABLE_DEFAULT   false
#endif
#ifdef CONFIG_BLIFI_RESET_INDICATOR_GPIO
#define BLIFI_RI_GPIO_DEFAULT     CONFIG_BLIFI_RESET_INDICATOR_GPIO
#else
#define BLIFI_RI_GPIO_DEFAULT     (-1)
#endif
#ifdef CONFIG_BLIFI_RESET_INDICATOR_ACTIVE_LOW
#define BLIFI_RI_LEVEL_DEFAULT    0
#else
#define BLIFI_RI_LEVEL_DEFAULT    1
#endif
#ifdef CONFIG_BLIFI_RESET_INDICATOR_PULSE_MS
#define BLIFI_RI_PULSE_DEFAULT    CONFIG_BLIFI_RESET_INDICATOR_PULSE_MS
#else
#define BLIFI_RI_PULSE_DEFAULT    2000
#endif

/* Kconfig-derived default for a fixed PoP; "" (the Kconfig default) means the PoP
 * is auto-generated on first boot and persisted in NVS, as before. */
#ifdef CONFIG_BLIFI_FIXED_POP
#define BLIFI_FIXED_POP_DEFAULT   CONFIG_BLIFI_FIXED_POP
#else
#define BLIFI_FIXED_POP_DEFAULT   ""
#endif

/** Top-level configuration. Prefer ::BLIFI_DEFAULT_CONFIG. */
typedef struct {
    const char *device_name;   /*!< BLE name; NULL → "blifi-XXXX" from the MAC */
    bool        require_pop;   /*!< Require Proof-of-Possession (default true) */
    const char *fixed_pop;     /*!< Non-empty = use this exact 8-char Crockford PoP
                                    instead of generating one / reading NVS.
                                    NULL or "" = auto-generate (the default). */
    bool        manage_nvs;    /*!< true (default): blifi_init initialises the default
                                    NVS partition itself (and erases + retries once if
                                    it is full or from an incompatible version), so the
                                    application need not call nvs_flash_init(). Set false
                                    if the application manages the default NVS partition
                                    itself; blifi then assumes it is already initialised. */
    uint32_t    prov_timeout_ms; /*!< If >0, stop advertising and post
                                    ::BLIFI_EVENT_PROV_TIMEOUT when no BLE central
                                    connects within this many ms of advertising.
                                    Re-arm with blifi_start(). 0 = no timeout. */
    blifi_wifi_manager_config_t wifi; /*!< Wi-Fi retry/backoff config */
    blifi_reset_indicator_config_t reset_indicator; /*!< Hard-reset indicator (§6.2) */
} blifi_config_t;

#define BLIFI_DEFAULT_CONFIG() ((blifi_config_t){ \
    .device_name     = NULL,  \
    .require_pop     = true,  \
    .fixed_pop       = BLIFI_FIXED_POP_DEFAULT, \
    .manage_nvs      = true,  \
    .prov_timeout_ms = 0,     \
    .wifi            = BLIFI_WIFI_MANAGER_DEFAULT_CONFIG(), \
    .reset_indicator = { \
        .enable       = BLIFI_RI_ENABLE_DEFAULT, \
        .gpio         = BLIFI_RI_GPIO_DEFAULT,   \
        .active_level = BLIFI_RI_LEVEL_DEFAULT,  \
        .pulse_ms     = BLIFI_RI_PULSE_DEFAULT,  \
    }, \
})

/**
 * @brief Initialise blifi: the default NVS partition (unless
 *        ::blifi_config_t.manage_nvs is false), the Wi-Fi manager, the PoP
 *        (generated on first boot and logged once over UART), and the BLE stack.
 *        With `manage_nvs` left at its default (true) the application does not
 *        need to call nvs_flash_init() itself.
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
 *        SPIFFS/LittleFS files, etc.). Wi-Fi credentials are already gone - the
 *        bootloader erased the dedicated `blifi_nvs` partition before app_main.
 * @param arg The opaque pointer passed to ::blifi_register_data_reset_callback.
 */
typedef void (*blifi_data_reset_cb_t)(void *arg);

/**
 * @brief Register an optional app-data reset callback for the hard-reset flow
 *        (§6.1). Call this **before** ::blifi_start (typically right before
 *        ::blifi_init); ::blifi_start fires it - and posts
 *        ::BLIFI_EVENT_HARD_RESET_TRIGGERED - when this boot follows a hard reset.
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
