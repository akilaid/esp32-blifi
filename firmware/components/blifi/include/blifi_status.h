/**
 * @file blifi_status.h
 * @brief Provisioning status / error codes shared across the blifi firmware
 *        and mirrored byte-for-byte by the Flutter package.
 *
 * Values MUST match docs/protocol-spec.md §7 exactly. `0x00`-`0x1F` are states,
 * `0x20`+ are errors.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Provisioning status / error code (single u8 space). */
typedef enum {
    /* States */
    BLIFI_STATUS_IDLE                   = 0x00, /*!< No provisioning in progress */
    BLIFI_STATUS_HANDSHAKE_IN_PROGRESS  = 0x01, /*!< BLE handshake underway (Phase 3) */
    BLIFI_STATUS_HANDSHAKE_OK           = 0x02, /*!< Session established (Phase 3) */
    BLIFI_STATUS_CREDENTIALS_RECEIVED   = 0x10, /*!< Credentials accepted */
    BLIFI_STATUS_WIFI_CONNECTING        = 0x11, /*!< Attempting to join the AP */
    BLIFI_STATUS_WIFI_CONNECTED         = 0x12, /*!< Online; IP acquired */

    /* Errors */
    BLIFI_STATUS_AUTH_FAILED            = 0x20, /*!< Wrong PoP / confirmation mismatch (Phase 3) */
    BLIFI_STATUS_WIFI_NOT_FOUND         = 0x21, /*!< Target SSID not found */
    BLIFI_STATUS_WIFI_AUTH_ERROR        = 0x22, /*!< Wrong Wi-Fi password */
    BLIFI_STATUS_WIFI_TIMEOUT           = 0x23, /*!< Association / DHCP timed out */
    BLIFI_STATUS_WIFI_DISCONNECTED      = 0x24, /*!< Lost connection after joining */
    BLIFI_STATUS_PROV_TIMEOUT           = 0x25, /*!< Provisioning window elapsed (Phase 3) */
    BLIFI_STATUS_INVALID_MESSAGE        = 0x26, /*!< Malformed / failed-auth frame (Phase 3) */
    BLIFI_STATUS_INTERNAL_ERROR         = 0x27, /*!< Unexpected device fault */
} blifi_status_t;

/**
 * @brief Human-readable name for a status code (for logs; never parse this).
 * @return Static string, e.g. "WIFI_CONNECTED"; "UNKNOWN" for unrecognised codes.
 */
const char *blifi_status_str(blifi_status_t status);

#ifdef __cplusplus
}
#endif
