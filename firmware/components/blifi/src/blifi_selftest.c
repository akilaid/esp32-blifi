/**
 * @file blifi_selftest.c
 * @brief Known-answer and round-trip self-tests for the crypto/framing core.
 */
#include "blifi_selftest.h"

#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "psa/crypto.h"

#include "crypto.h"
#include "frame.h"
#include "messages.h"
#include "pop.h"

static const char *TAG = "blifi_test";

static int g_fail;

#define CHECK(cond, name)                             \
    do {                                              \
        if (cond) {                                   \
            ESP_LOGI(TAG, "PASS  %s", name);          \
        } else {                                      \
            ESP_LOGE(TAG, "FAIL  %s", name);          \
            g_fail++;                                 \
        }                                             \
    } while (0)

/* ---- RFC 7748 §5.2 X25519 known-answer ---- */
static void test_x25519(void)
{
    static const uint8_t scalar[32] = {
        0xa5,0x46,0xe3,0x6b,0xf0,0x52,0x7c,0x9d,0x3b,0x16,0x15,0x4b,0x82,0x46,0x5e,0xdd,
        0x62,0x14,0x4c,0x0a,0xc1,0xfc,0x5a,0x18,0x50,0x6a,0x22,0x44,0xba,0x44,0x9a,0xc4};
    static const uint8_t u[32] = {
        0xe6,0xdb,0x68,0x67,0x58,0x30,0x30,0xdb,0x35,0x94,0xc1,0xa4,0x24,0xb1,0x5f,0x7c,
        0x72,0x66,0x24,0xec,0x26,0xb3,0x35,0x3b,0x10,0xa9,0x03,0xa6,0xd0,0xab,0x1c,0x4c};
    static const uint8_t expect[32] = {
        0xc3,0xda,0x55,0x37,0x9d,0xe9,0xc6,0x90,0x8e,0x94,0xea,0x4d,0xf2,0x8d,0x08,0x4f,
        0x32,0xec,0xcf,0x03,0x49,0x1c,0x71,0xf7,0x54,0xb4,0x07,0x55,0x77,0xa2,0x85,0x52};
    uint8_t out[32];
    CHECK(blifi_crypto_x25519(scalar, u, out) == ESP_OK &&
          memcmp(out, expect, 32) == 0, "X25519 RFC7748 vector");
}

/* ---- ECDH agreement (proves keygen + shared-secret symmetry) ---- */
static void test_ecdh_agreement(void)
{
    uint8_t a_priv[32], b_priv[32], a_pub[32], b_pub[32], s1[32], s2[32];
    esp_fill_random(a_priv, 32);
    esp_fill_random(b_priv, 32);
    blifi_crypto_x25519(a_priv, NULL, a_pub);
    blifi_crypto_x25519(b_priv, NULL, b_pub);
    blifi_crypto_x25519(a_priv, b_pub, s1);
    blifi_crypto_x25519(b_priv, a_pub, s2);
    CHECK(memcmp(s1, s2, 32) == 0, "X25519 ECDH agreement");
}

/* ---- RFC 5869 HKDF-SHA256 Test Case 1 (via PSA) ---- */
static void test_hkdf(void)
{
    uint8_t ikm[22];
    memset(ikm, 0x0b, sizeof(ikm));
    static const uint8_t salt[13] = {0,1,2,3,4,5,6,7,8,9,10,11,12};
    static const uint8_t info[10] = {0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9};
    static const uint8_t okm_ok[42] = {
        0x3c,0xb2,0x5f,0x25,0xfa,0xac,0xd5,0x7a,0x90,0x43,0x4f,0x64,0xd0,0x36,0x2f,0x2a,
        0x2d,0x2d,0x0a,0x90,0xcf,0x1a,0x5a,0x4c,0x5d,0xb0,0x2d,0x56,0xec,0xc4,0xc5,0xbf,
        0x34,0x00,0x72,0x08,0xd5,0xb8,0x87,0x18,0x58,0x65};

    uint8_t okm[42];
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t st = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (st == PSA_SUCCESS) st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, sizeof(salt));
    if (st == PSA_SUCCESS) st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm, sizeof(ikm));
    if (st == PSA_SUCCESS) st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, info, sizeof(info));
    if (st == PSA_SUCCESS) st = psa_key_derivation_output_bytes(&op, okm, sizeof(okm));
    psa_key_derivation_abort(&op);
    CHECK(st == PSA_SUCCESS && memcmp(okm, okm_ok, sizeof(okm)) == 0, "HKDF-SHA256 (RFC5869)");
}

/* ---- AES-256-GCM known-answer (independent vector, plaintext + AAD) ---- */
static void test_gcm(void)
{
    static const uint8_t key[32] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f};
    static const uint8_t iv[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    static const uint8_t aad[2] = {0x01, 0x30};
    static const uint8_t pt[14] = {0x62,0x6c,0x69,0x66,0x69,0x20,0x67,0x63,0x6d,0x20,0x6b,0x61,0x74,0x21};
    static const uint8_t expect[30] = { /* ciphertext(14) ‖ tag(16) */
        0x25,0x6e,0xbf,0x7d,0xac,0xc5,0xa5,0x78,0xe0,0x61,0xfc,0xea,0xc5,0xc8,
        0x84,0x19,0x67,0x20,0xc9,0x72,0x13,0x45,0x8f,0x3c,0x57,0xa5,0x23,0x69,0x83,0x69};

    psa_key_attributes_t a = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&a, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&a, 256);
    psa_set_key_usage_flags(&a, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&a, PSA_ALG_GCM);
    psa_key_id_t k;
    uint8_t out[30];
    size_t olen = 0;
    psa_status_t st = psa_import_key(&a, key, sizeof(key), &k);
    if (st == PSA_SUCCESS) {
        st = psa_aead_encrypt(k, PSA_ALG_GCM, iv, sizeof(iv), aad, sizeof(aad),
                              pt, sizeof(pt), out, sizeof(out), &olen);
        psa_destroy_key(k);
    }
    CHECK(st == PSA_SUCCESS && olen == sizeof(expect) &&
          memcmp(out, expect, sizeof(expect)) == 0, "AES-256-GCM KAT");
}

/* ---- Full session encrypt/decrypt, AAD binding, replay ---- */
static void test_session_roundtrip(void)
{
    blifi_session_t dev;
    if (blifi_crypto_session_init(&dev) != ESP_OK) {
        CHECK(false, "session init");
        return;
    }
    uint8_t app_priv[32], app_pub[32];
    esp_fill_random(app_priv, 32);
    blifi_crypto_x25519(app_priv, NULL, app_pub);
    CHECK(blifi_crypto_derive(&dev, app_pub, "TESTPOP1") == ESP_OK, "session derive");

    /* Mirror the app side by swapping the directional keys. */
    blifi_session_t app = {0};
    app.established = true;
    memcpy(app.k_app2dev, dev.k_dev2app, 32); /* app decrypts dev→app with k_dev2app */
    memcpy(app.k_dev2app, dev.k_app2dev, 32); /* app encrypts app→dev with k_app2dev */

    const char *msg = "hello blifi";
    uint8_t rec[64], pt[64];
    size_t rlen = 0, plen = 0;
    esp_err_t e1 = blifi_crypto_encrypt(&dev, BLIFI_MSG_STATUS, (const uint8_t *)msg,
                                        strlen(msg), rec, sizeof(rec), &rlen);
    esp_err_t e2 = blifi_crypto_decrypt(&app, BLIFI_MSG_STATUS, rec, rlen, pt, sizeof(pt), &plen);
    CHECK(e1 == ESP_OK && e2 == ESP_OK && plen == strlen(msg) &&
          memcmp(pt, msg, plen) == 0, "session encrypt/decrypt round-trip");

    /* Wrong msg_type in AAD must fail authentication. */
    esp_err_t e3 = blifi_crypto_decrypt(&app, BLIFI_MSG_CREDENTIALS, rec, rlen, pt, sizeof(pt), &plen);
    CHECK(e3 != ESP_OK, "AAD mismatch rejected");

    /* Replaying the same record (counter not advancing) must fail. */
    blifi_session_t app2 = {0};
    app2.established = true;
    memcpy(app2.k_app2dev, dev.k_dev2app, 32);
    blifi_crypto_decrypt(&app2, BLIFI_MSG_STATUS, rec, rlen, pt, sizeof(pt), &plen); /* first ok */
    esp_err_t e4 = blifi_crypto_decrypt(&app2, BLIFI_MSG_STATUS, rec, rlen, pt, sizeof(pt), &plen);
    CHECK(e4 == ESP_ERR_INVALID_STATE, "replay rejected");

    /* app→dev direction. */
    uint8_t rec2[64], pt2[64];
    size_t r2len = 0, p2len = 0;
    esp_err_t e5 = blifi_crypto_encrypt(&app, BLIFI_MSG_CREDENTIALS, (const uint8_t *)msg,
                                        strlen(msg), rec2, sizeof(rec2), &r2len);
    esp_err_t e6 = blifi_crypto_decrypt(&dev, BLIFI_MSG_CREDENTIALS, rec2, r2len, pt2, sizeof(pt2), &p2len);
    CHECK(e5 == ESP_OK && e6 == ESP_OK && p2len == strlen(msg) &&
          memcmp(pt2, msg, p2len) == 0, "app→dev encrypt/decrypt");
}

/* ---- Frame chunk → reassemble ---- */
typedef struct {
    blifi_reasm_t r;
    bool complete;
    bool error;
} reasm_ctx_t;

static esp_err_t emit_to_reasm(void *ctx, const uint8_t *frame, size_t len)
{
    reasm_ctx_t *c = ctx;
    blifi_reasm_status_t st = blifi_reasm_feed(&c->r, frame, len);
    if (st == BLIFI_REASM_ERROR) c->error = true;
    if (st == BLIFI_REASM_COMPLETE) c->complete = true;
    return ESP_OK;
}

static void test_framing(void)
{
    uint8_t payload[200];
    for (int i = 0; i < (int)sizeof(payload); i++) {
        payload[i] = (uint8_t)(i * 7 + 1);
    }
    reasm_ctx_t ctx = {0};
    esp_err_t e = blifi_frame_send(BLIFI_MSG_SCAN_RESPONSE, payload, sizeof(payload), 40,
                                   emit_to_reasm, &ctx);
    CHECK(e == ESP_OK && ctx.complete && !ctx.error &&
          ctx.r.msg_type == BLIFI_MSG_SCAN_RESPONSE &&
          ctx.r.total_len == sizeof(payload) &&
          memcmp(ctx.r.buf, payload, sizeof(payload)) == 0, "frame chunk/reassemble (multi)");

    /* Single-frame case. */
    reasm_ctx_t ctx2 = {0};
    uint8_t small[10] = {1,2,3,4,5,6,7,8,9,10};
    blifi_frame_send(BLIFI_MSG_HS_PUBKEY, small, sizeof(small), 100, emit_to_reasm, &ctx2);
    CHECK(ctx2.complete && ctx2.r.total_len == sizeof(small) &&
          memcmp(ctx2.r.buf, small, sizeof(small)) == 0, "frame single-chunk");
}

/* ---- JSON credentials decode (escaped password, optional bssid) ---- */
static void test_json(void)
{
    const char *j1 = "{\"ssid\":\"Home WiFi\",\"password\":\"p@ss\\\"w\\\\rd\"}";
    blifi_wifi_credentials_t c;
    esp_err_t e = blifi_msg_credentials_decode((const uint8_t *)j1, strlen(j1), &c);
    CHECK(e == ESP_OK && strcmp(c.ssid, "Home WiFi") == 0 &&
          strcmp(c.password, "p@ss\"w\\rd") == 0 && !c.bssid_set,
          "JSON credentials decode (escapes)");

    const char *j2 = "{\"ssid\":\"Net\",\"password\":\"x\",\"bssid\":\"aa:bb:cc:dd:ee:ff\",\"channel\":6}";
    blifi_wifi_credentials_t c2;
    e = blifi_msg_credentials_decode((const uint8_t *)j2, strlen(j2), &c2);
    CHECK(e == ESP_OK && c2.bssid_set && c2.channel == 6 &&
          c2.bssid[0] == 0xaa && c2.bssid[5] == 0xff, "JSON credentials decode (bssid hint)");

    const char *j3 = "{\"ssid\":\"\"}"; /* empty ssid → reject */
    blifi_wifi_credentials_t c3;
    CHECK(blifi_msg_credentials_decode((const uint8_t *)j3, strlen(j3), &c3) != ESP_OK,
          "JSON credentials reject empty ssid");
}

static void test_pop_validate(void)
{
    CHECK(blifi_pop_validate("ABCD2345") == ESP_OK, "PoP validate accepts valid");
    CHECK(blifi_pop_validate("K7M2QP9X") == ESP_OK, "PoP validate accepts valid 2");
    CHECK(blifi_pop_validate("ABC") != ESP_OK, "PoP validate rejects too short");
    CHECK(blifi_pop_validate("ABCD23456") != ESP_OK, "PoP validate rejects too long");
    CHECK(blifi_pop_validate("ILOU2345") != ESP_OK, "PoP validate rejects I/L/O/U");
    CHECK(blifi_pop_validate("TESTPOP1") != ESP_OK, "PoP validate rejects O (Crockford excludes it)");
    CHECK(blifi_pop_validate("abcd2345") != ESP_OK, "PoP validate rejects lowercase");
    CHECK(blifi_pop_validate(NULL) != ESP_OK, "PoP validate rejects NULL");
}

int blifi_selftest(void)
{
    g_fail = 0;
    ESP_LOGI(TAG, "--- blifi self-test ---");
    psa_crypto_init();
    test_x25519();
    test_ecdh_agreement();
    test_hkdf();
    test_gcm();
    test_session_roundtrip();
    test_framing();
    test_json();
    test_pop_validate();
    if (g_fail == 0) {
        ESP_LOGI(TAG, "--- ALL PASSED ---");
    } else {
        ESP_LOGE(TAG, "--- %d CHECK(S) FAILED ---", g_fail);
    }
    return g_fail;
}
