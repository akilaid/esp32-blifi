/**
 * @file crypto.h
 * @brief blifi session crypto: X25519 + HKDF + AES-256-GCM + HMAC confirmation.
 *        Implements docs/security.md §4-§6 and protocol-spec.md §5 exactly.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLIFI_KEY_LEN     32  /*!< X25519 key / AES-256 key / session key */
#define BLIFI_GCM_TAG_LEN 16
#define BLIFI_CONFIRM_LEN 16  /*!< truncated HMAC confirmation tag */
#define BLIFI_COUNTER_LEN 4
/** Encrypted-record overhead: counter(4) + tag(16). */
#define BLIFI_RECORD_OVERHEAD (BLIFI_COUNTER_LEN + BLIFI_GCM_TAG_LEN)

/** One provisioning session. `device` = responder, `app` = initiator. */
typedef struct {
    uint32_t device_key;  /*!< psa_key_id_t of the device's ephemeral X25519 pair */
    uint8_t  device_pub[BLIFI_KEY_LEN];
    uint8_t  app_pub[BLIFI_KEY_LEN];

    uint8_t  k_app2dev[BLIFI_KEY_LEN]; /*!< decrypt app→dev */
    uint8_t  k_dev2app[BLIFI_KEY_LEN]; /*!< encrypt dev→app */
    uint8_t  k_confirm[BLIFI_KEY_LEN];

    uint32_t tx_counter;      /*!< next dev→app counter */
    uint32_t rx_counter_last; /*!< last accepted app→dev counter */
    bool     rx_seen;         /*!< any app→dev record accepted yet */
    bool     established;     /*!< keys derived */
} blifi_session_t;

/** Generate the device's ephemeral X25519 keypair; fills `device_pub`. */
esp_err_t blifi_crypto_session_init(blifi_session_t *s);

/** Release the session's PoP key material (destroys the ephemeral PSA key). */
void blifi_crypto_session_free(blifi_session_t *s);

/**
 * @brief Raw X25519: `out = clamp(scalar) * point` (RFC 7748 little-endian).
 *        `point == NULL` uses the curve base point (i.e. derive a public key).
 *        Exposed for known-answer self-tests.
 */
esp_err_t blifi_crypto_x25519(const uint8_t scalar[BLIFI_KEY_LEN],
                              const uint8_t *point, uint8_t out[BLIFI_KEY_LEN]);

/**
 * @brief Derive session keys from the app's public key and the PoP.
 * @param app_pub 32-byte X25519 public key from the app.
 * @param pop     PoP string; pass "" for the no-PoP dev mode.
 */
esp_err_t blifi_crypto_derive(blifi_session_t *s, const uint8_t app_pub[BLIFI_KEY_LEN],
                              const char *pop);

/** Compute the app's expected confirmation tag (device verifies against this). */
esp_err_t blifi_crypto_confirm_app(const blifi_session_t *s, uint8_t out[BLIFI_CONFIRM_LEN]);
/** Compute the device's confirmation tag (sent to the app on success). */
esp_err_t blifi_crypto_confirm_dev(const blifi_session_t *s, uint8_t out[BLIFI_CONFIRM_LEN]);

/**
 * @brief Encrypt a dev→app message into a record `counter‖ciphertext‖tag`.
 *        Uses and increments the tx counter. `out` needs
 *        `pt_len + BLIFI_RECORD_OVERHEAD` bytes.
 */
esp_err_t blifi_crypto_encrypt(blifi_session_t *s, uint8_t msg_type,
                               const uint8_t *pt, size_t pt_len,
                               uint8_t *out, size_t cap, size_t *out_len);

/**
 * @brief Decrypt an app→dev record. Enforces the replay rule (counter strictly
 *        increasing). `out` needs `rec_len - BLIFI_RECORD_OVERHEAD` bytes.
 * @return ESP_OK, ESP_ERR_INVALID_CRC on tag failure, ESP_ERR_INVALID_STATE on replay.
 */
esp_err_t blifi_crypto_decrypt(blifi_session_t *s, uint8_t msg_type,
                               const uint8_t *record, size_t rec_len,
                               uint8_t *out, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif
