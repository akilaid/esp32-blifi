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
} blifi_event_id_t;

/** Data payload accompanying a ::BLIFI_EVENT post. */
typedef struct {
    blifi_status_t status;      /*!< Current status code */
    int            wifi_reason; /*!< Raw wifi_err_reason_t on failures, else 0 */
    esp_ip4_addr_t ip;          /*!< Valid on ::BLIFI_EVENT_WIFI_CONNECTED */
} blifi_event_data_t;

#ifdef __cplusplus
}
#endif
