/**
 * @file ble_transport.h
 * @brief NimBLE GATT server for the blifi provisioning service (protocol-spec §2).
 *        Handles advertising, framing/chunking, and reassembly; hands complete
 *        messages up to the provisioning manager.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Logical characteristics (protocol-spec §2). */
typedef enum {
    BLIFI_CH_DEVICE_INFO = 0,
    BLIFI_CH_HANDSHAKE,
    BLIFI_CH_SCAN,
    BLIFI_CH_CREDENTIALS,
    BLIFI_CH_STATUS,
    BLIFI_CH_COUNT,
} blifi_ble_char_t;

/** A complete reassembled inbound message. */
typedef void (*blifi_ble_msg_cb)(void *ctx, blifi_ble_char_t ch, uint8_t msg_type,
                                 const uint8_t *payload, size_t len);
/** Connection state change. */
typedef void (*blifi_ble_conn_cb)(void *ctx, bool connected);
/** Fill the plaintext Device-Info payload for a read; returns bytes written. */
typedef size_t (*blifi_ble_devinfo_cb)(void *ctx, uint8_t *out, size_t cap);

typedef struct {
    const char           *device_name; /*!< NULL → "blifi-XXXX" from the MAC */
    blifi_ble_msg_cb      on_message;
    blifi_ble_conn_cb     on_conn;
    blifi_ble_devinfo_cb  on_devinfo_read;
    void                 *ctx;
} blifi_ble_config_t;

/** @brief Start the BLE stack, register the GATT service, and advertise. */
esp_err_t blifi_ble_start(const blifi_ble_config_t *config);

/** @brief Stop advertising and disconnect. The NimBLE host stays initialised;
 *         call ::blifi_ble_start again is NOT required to resume advertising. */
esp_err_t blifi_ble_stop(void);

/** @brief Full teardown: stop advertising, disconnect, and deinitialise the
 *         NimBLE host (frees its RAM). After this, ::blifi_ble_start must be
 *         called to bring BLE back. MUST NOT be called from the NimBLE host task
 *         (it stops and joins that task); call from another task context. */
esp_err_t blifi_ble_shutdown(void);

/** @brief Frame `payload` and notify it on characteristic `ch` (chunked to MTU). */
esp_err_t blifi_ble_send(blifi_ble_char_t ch, uint8_t msg_type,
                         const uint8_t *payload, size_t len);

/** @brief True while a central is connected. */
bool blifi_ble_is_connected(void);

/** @brief Resolve the advertised device name (e.g. for Device-Info). */
const char *blifi_ble_device_name(void);

#ifdef __cplusplus
}
#endif
