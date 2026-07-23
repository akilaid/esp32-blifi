/**
 * @file messages.h
 * @brief Message types and JSON payload codecs (docs/protocol-spec.md §4, §6).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#include "blifi_status.h"
#include "blifi_wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Message types (§4). */
typedef enum {
    BLIFI_MSG_DEVICE_INFO   = 0x01,
    BLIFI_MSG_HS_PUBKEY     = 0x02,
    BLIFI_MSG_HS_CONFIRM    = 0x03,
    BLIFI_MSG_HS_FAIL       = 0x04,
    BLIFI_MSG_SCAN_REQUEST  = 0x10,
    BLIFI_MSG_SCAN_RESPONSE = 0x11,
    BLIFI_MSG_CREDENTIALS   = 0x20,
    BLIFI_MSG_STATUS        = 0x30,
} blifi_msg_type_t;

/** Encode DEVICE_INFO JSON (§6.1) into `out`. */
esp_err_t blifi_msg_device_info_encode(const char *fw, const char *name,
                                       const char *state, bool pop_required,
                                       uint8_t *out, size_t cap, size_t *out_len);

/** Encode SCAN_RESPONSE JSON (§6.3) from an AP array into `out`. */
esp_err_t blifi_msg_scan_response_encode(const blifi_wifi_ap_t *aps, size_t n,
                                         uint8_t *out, size_t cap, size_t *out_len);

/** Encode STATUS JSON (§6.5); `ip` may be NULL. */
esp_err_t blifi_msg_status_encode(blifi_status_t code, const char *detail,
                                  const esp_ip4_addr_t *ip,
                                  uint8_t *out, size_t cap, size_t *out_len);

/** Decode SCAN_REQUEST JSON (§6.2). `refresh` may be NULL. */
esp_err_t blifi_msg_scan_request_decode(const uint8_t *json, size_t len, bool *refresh);

/** Decode CREDENTIALS JSON (§6.4) into `out`. */
esp_err_t blifi_msg_credentials_decode(const uint8_t *json, size_t len,
                                       blifi_wifi_credentials_t *out);

#ifdef __cplusplus
}
#endif
