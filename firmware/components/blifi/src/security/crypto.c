/**
 * @file crypto.c
 * @brief blifi session crypto over the PSA Crypto API (mbedTLS 4.x / TF-PSA).
 *        X25519 + HKDF-SHA256 + AES-256-GCM (PSA); HMAC-SHA256 via mbedtls md.
 *        Implements docs/security.md §4-§6 and protocol-spec.md §5.
 */
#include "crypto.h"
#include "frame.h" /* BLIFI_PROTO_VERSION */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"

#include "psa/crypto.h"

static const char *TAG = "blifi_crypto";

#define POP_MAX 64

/* Zeroize that the compiler may not optimize away. */
static void secure_zero(void *p, size_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n--) {
        *v++ = 0;
    }
}

static esp_err_t ensure_psa(void)
{
    static bool inited;
    if (inited) {
        return ESP_OK;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    inited = true;
    return ESP_OK;
}

esp_err_t blifi_crypto_x25519(const uint8_t scalar[BLIFI_KEY_LEN],
                              const uint8_t *point, uint8_t out[BLIFI_KEY_LEN])
{
    if (!scalar || !out || ensure_psa() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&attr, 255);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDH);

    psa_key_id_t k;
    if (psa_import_key(&attr, scalar, BLIFI_KEY_LEN, &k) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    size_t olen = 0;
    psa_status_t st;
    if (point == NULL) {
        st = psa_export_public_key(k, out, BLIFI_KEY_LEN, &olen);
    } else {
        st = psa_raw_key_agreement(PSA_ALG_ECDH, k, point, BLIFI_KEY_LEN, out, BLIFI_KEY_LEN, &olen);
    }
    psa_destroy_key(k);
    return (st == PSA_SUCCESS && olen == BLIFI_KEY_LEN) ? ESP_OK : ESP_FAIL;
}

esp_err_t blifi_crypto_session_init(blifi_session_t *s)
{
    if (!s || ensure_psa() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(s, 0, sizeof(*s));

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&attr, 255);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDH);

    psa_key_id_t k;
    if (psa_generate_key(&attr, &k) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    size_t olen = 0;
    if (psa_export_public_key(k, s->device_pub, BLIFI_KEY_LEN, &olen) != PSA_SUCCESS ||
        olen != BLIFI_KEY_LEN) {
        psa_destroy_key(k);
        return ESP_FAIL;
    }
    s->device_key = k;
    return ESP_OK;
}

void blifi_crypto_session_free(blifi_session_t *s)
{
    if (s && s->device_key) {
        psa_destroy_key(s->device_key);
        s->device_key = 0;
    }
    if (s) {
        s->established = false;
    }
}

/* One-shot HKDF-SHA256; equal to expand(extract(salt,ikm), info). */
static esp_err_t hkdf(const uint8_t *salt, size_t salt_len, const uint8_t *ikm,
                      size_t ikm_len, const char *info, uint8_t out[BLIFI_KEY_LEN])
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t st = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_len);
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm, ikm_len);
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                            (const uint8_t *)info, strlen(info));
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_output_bytes(&op, out, BLIFI_KEY_LEN);
    }
    psa_key_derivation_abort(&op);
    return st == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
}

esp_err_t blifi_crypto_derive(blifi_session_t *s, const uint8_t app_pub[BLIFI_KEY_LEN],
                              const char *pop)
{
    if (!s || !app_pub || !s->device_key) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s->app_pub, app_pub, BLIFI_KEY_LEN);

    uint8_t ecdh[BLIFI_KEY_LEN];
    size_t olen = 0;
    if (psa_raw_key_agreement(PSA_ALG_ECDH, s->device_key, app_pub, BLIFI_KEY_LEN,
                              ecdh, sizeof(ecdh), &olen) != PSA_SUCCESS || olen != BLIFI_KEY_LEN) {
        return ESP_FAIL;
    }

    /* salt = app_pub ‖ device_pub */
    uint8_t salt[2 * BLIFI_KEY_LEN];
    memcpy(salt, s->app_pub, BLIFI_KEY_LEN);
    memcpy(salt + BLIFI_KEY_LEN, s->device_pub, BLIFI_KEY_LEN);

    /* IKM = ecdh ‖ utf8(pop) */
    size_t pop_len = pop ? strnlen(pop, POP_MAX) : 0;
    uint8_t ikm[BLIFI_KEY_LEN + POP_MAX];
    memcpy(ikm, ecdh, BLIFI_KEY_LEN);
    if (pop_len) {
        memcpy(ikm + BLIFI_KEY_LEN, pop, pop_len);
    }

    esp_err_t err = hkdf(salt, sizeof(salt), ikm, BLIFI_KEY_LEN + pop_len,
                         "blifi/v1 key app->dev", s->k_app2dev);
    if (err == ESP_OK) err = hkdf(salt, sizeof(salt), ikm, BLIFI_KEY_LEN + pop_len,
                                  "blifi/v1 key dev->app", s->k_dev2app);
    if (err == ESP_OK) err = hkdf(salt, sizeof(salt), ikm, BLIFI_KEY_LEN + pop_len,
                                  "blifi/v1 confirm", s->k_confirm);

    secure_zero(ecdh, sizeof(ecdh));
    secure_zero(ikm, sizeof(ikm));
    if (err != ESP_OK) {
        return err;
    }
    s->tx_counter = 0;
    s->rx_counter_last = 0;
    s->rx_seen = false;
    s->established = true;
    return ESP_OK;
}

static esp_err_t confirm_tag(const blifi_session_t *s, const char *label,
                             uint8_t out[BLIFI_CONFIRM_LEN])
{
    uint8_t msg[32 + 2 * BLIFI_KEY_LEN];
    size_t label_len = strlen(label);
    if (label_len > 32) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(msg, label, label_len);
    memcpy(msg + label_len, s->app_pub, BLIFI_KEY_LEN);
    memcpy(msg + label_len + BLIFI_KEY_LEN, s->device_pub, BLIFI_KEY_LEN);

    if (ensure_psa() != ESP_OK) {
        return ESP_FAIL;
    }
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, BLIFI_KEY_LEN * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_key_id_t k;
    if (psa_import_key(&attr, s->k_confirm, BLIFI_KEY_LEN, &k) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    uint8_t full[32];
    size_t olen = 0;
    psa_status_t st = psa_mac_compute(k, PSA_ALG_HMAC(PSA_ALG_SHA_256), msg,
                                      label_len + 2 * BLIFI_KEY_LEN, full, sizeof(full), &olen);
    psa_destroy_key(k);
    if (st != PSA_SUCCESS || olen < BLIFI_CONFIRM_LEN) {
        return ESP_FAIL;
    }
    memcpy(out, full, BLIFI_CONFIRM_LEN);
    return ESP_OK;
}

esp_err_t blifi_crypto_confirm_app(const blifi_session_t *s, uint8_t out[BLIFI_CONFIRM_LEN])
{
    return confirm_tag(s, "blifi/v1 confirm app", out);
}

esp_err_t blifi_crypto_confirm_dev(const blifi_session_t *s, uint8_t out[BLIFI_CONFIRM_LEN])
{
    return confirm_tag(s, "blifi/v1 confirm dev", out);
}

static void nonce_from_counter(uint32_t counter, uint8_t nonce[12])
{
    memset(nonce, 0, 12);
    nonce[8]  = (uint8_t)(counter >> 24);
    nonce[9]  = (uint8_t)(counter >> 16);
    nonce[10] = (uint8_t)(counter >> 8);
    nonce[11] = (uint8_t)(counter);
}

static esp_err_t aes_import(const uint8_t key[BLIFI_KEY_LEN], psa_key_usage_t usage,
                            psa_key_id_t *out)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 256);
    psa_set_key_usage_flags(&attr, usage);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);
    return psa_import_key(&attr, key, BLIFI_KEY_LEN, out) == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
}

esp_err_t blifi_crypto_encrypt(blifi_session_t *s, uint8_t msg_type,
                               const uint8_t *pt, size_t pt_len,
                               uint8_t *out, size_t cap, size_t *out_len)
{
    if (!s || !s->established || (!pt && pt_len) || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cap < pt_len + BLIFI_RECORD_OVERHEAD || ensure_psa() != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t counter = s->tx_counter;
    uint8_t nonce[12];
    nonce_from_counter(counter, nonce);
    uint8_t aad[2] = { BLIFI_PROTO_VERSION, msg_type };

    out[0] = (uint8_t)(counter >> 24);
    out[1] = (uint8_t)(counter >> 16);
    out[2] = (uint8_t)(counter >> 8);
    out[3] = (uint8_t)(counter);

    psa_key_id_t k;
    if (aes_import(s->k_dev2app, PSA_KEY_USAGE_ENCRYPT, &k) != ESP_OK) {
        return ESP_FAIL;
    }
    size_t olen = 0;
    psa_status_t st = psa_aead_encrypt(k, PSA_ALG_GCM, nonce, sizeof(nonce), aad, sizeof(aad),
                                       pt, pt_len, out + BLIFI_COUNTER_LEN,
                                       cap - BLIFI_COUNTER_LEN, &olen);
    psa_destroy_key(k);
    if (st != PSA_SUCCESS) {
        ESP_LOGE(TAG, "aead encrypt failed: %d", (int)st);
        return ESP_FAIL;
    }
    s->tx_counter++;
    if (out_len) {
        *out_len = BLIFI_COUNTER_LEN + olen;
    }
    return ESP_OK;
}

esp_err_t blifi_crypto_decrypt(blifi_session_t *s, uint8_t msg_type,
                               const uint8_t *record, size_t rec_len,
                               uint8_t *out, size_t cap, size_t *out_len)
{
    if (!s || !s->established || !record || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (rec_len < BLIFI_RECORD_OVERHEAD || ensure_psa() != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t ct_len = rec_len - BLIFI_COUNTER_LEN; /* ciphertext + tag for PSA */
    if (cap < rec_len - BLIFI_RECORD_OVERHEAD) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t counter = ((uint32_t)record[0] << 24) | ((uint32_t)record[1] << 16) |
                       ((uint32_t)record[2] << 8) | record[3];
    if (s->rx_seen && counter <= s->rx_counter_last) {
        ESP_LOGW(TAG, "replay: counter %" PRIu32 " <= %" PRIu32, counter, s->rx_counter_last);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t nonce[12];
    nonce_from_counter(counter, nonce);
    uint8_t aad[2] = { BLIFI_PROTO_VERSION, msg_type };

    psa_key_id_t k;
    if (aes_import(s->k_app2dev, PSA_KEY_USAGE_DECRYPT, &k) != ESP_OK) {
        return ESP_FAIL;
    }
    size_t olen = 0;
    psa_status_t st = psa_aead_decrypt(k, PSA_ALG_GCM, nonce, sizeof(nonce), aad, sizeof(aad),
                                       record + BLIFI_COUNTER_LEN, ct_len, out, cap, &olen);
    psa_destroy_key(k);
    if (st != PSA_SUCCESS) {
        ESP_LOGW(TAG, "aead decrypt/auth failed: %d", (int)st);
        return ESP_ERR_INVALID_CRC;
    }
    s->rx_counter_last = counter;
    s->rx_seen = true;
    if (out_len) {
        *out_len = olen;
    }
    return ESP_OK;
}
