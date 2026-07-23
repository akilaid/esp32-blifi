/**
 * @file provisioning_manager.c
 * @brief Orchestrates BLE transport + session crypto + wifi_manager and
 *        implements the public blifi API (docs/plan.md §6, protocol-spec §8).
 */
#include "blifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include "ble_transport.h"
#include "crypto.h"
#include "frame.h"
#include "messages.h"
#include "pop.h"

static const char *TAG = "blifi_prov";

#define NVS_NS      "blifi"
#define NVS_KEY_POP "pop"
#define LOCKOUT_FAILS 5
#define LOCKOUT_US    (30 * 1000 * 1000)

typedef enum { ST_UNPROV, ST_PROV, ST_CONNECTING, ST_CONNECTED } prov_state_t;

static struct {
    bool               inited;
    bool               ble_started;
    blifi_config_t     cfg;
    char               pop[BLIFI_POP_BUFSZ];
    prov_state_t       state;
    blifi_session_t    session;
    bool               session_active;
    bool               established;
    int                fail_count;
    int64_t            lockout_until;
    SemaphoreHandle_t  lock;
} s;

static uint8_t s_scratch[2048];      /* protected by s.lock */
static blifi_wifi_ap_t s_aps[20];

#define LOCK()   xSemaphoreTakeRecursive(s.lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(s.lock)

static const char *state_str(prov_state_t st)
{
    switch (st) {
    case ST_UNPROV:     return "unprovisioned";
    case ST_PROV:       return "provisioning";
    case ST_CONNECTING: return "connecting";
    case ST_CONNECTED:  return "connected";
    default:            return "unknown";
    }
}

/* Constant-time compare. */
static bool ct_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++) {
        d |= a[i] ^ b[i];
    }
    return d == 0;
}

/* ------------------------------------------------------------------- PoP */

static void pop_load_or_create(void)
{
    if (!s.cfg.require_pop) {
        s.pop[0] = '\0';
        ESP_LOGW(TAG, "PoP DISABLED (dev mode) — not for production");
        return;
    }
    nvs_handle_t h;
    size_t len = sizeof(s.pop);
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        esp_err_t e = nvs_get_str(h, NVS_KEY_POP, s.pop, &len);
        nvs_close(h);
        if (e == ESP_OK) {
            ESP_LOGI(TAG, "Proof-of-Possession (stored): %s", s.pop);
            return;
        }
    }
    if (blifi_pop_generate(s.pop, sizeof(s.pop)) != ESP_OK) {
        ESP_LOGE(TAG, "PoP generation failed");
        return;
    }
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_POP, s.pop);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGW(TAG, "==================================");
    ESP_LOGW(TAG, " Proof-of-Possession: %s", s.pop);
    ESP_LOGW(TAG, "==================================");
}

/* -------------------------------------------------------- encrypted send */

static esp_err_t send_encrypted(blifi_ble_char_t ch, uint8_t msg_type,
                                const uint8_t *json, size_t jlen)
{
    uint8_t rec[sizeof(s_scratch) + BLIFI_RECORD_OVERHEAD];
    size_t rlen = 0;
    esp_err_t err = blifi_crypto_encrypt(&s.session, msg_type, json, jlen,
                                         rec, sizeof(rec), &rlen);
    if (err != ESP_OK) {
        return err;
    }
    return blifi_ble_send(ch, msg_type, rec, rlen);
}

static void send_status(blifi_status_t code, const esp_ip4_addr_t *ip)
{
    if (!s.established) {
        return;
    }
    size_t n = 0;
    if (blifi_msg_status_encode(code, blifi_status_str(code), ip,
                                s_scratch, sizeof(s_scratch), &n) == ESP_OK) {
        send_encrypted(BLIFI_CH_STATUS, BLIFI_MSG_STATUS, s_scratch, n);
    }
}

/* --------------------------------------------------------- BLE handlers */

static void handle_handshake(uint8_t msg_type, const uint8_t *payload, size_t len)
{
    if (msg_type == BLIFI_MSG_HS_PUBKEY) {
        if (!s.session_active || len != BLIFI_KEY_LEN) {
            return;
        }
        const char *pop = s.cfg.require_pop ? s.pop : "";
        if (blifi_crypto_derive(&s.session, payload, pop) != ESP_OK) {
            ESP_LOGE(TAG, "key derivation failed");
            return;
        }
        blifi_ble_send(BLIFI_CH_HANDSHAKE, BLIFI_MSG_HS_PUBKEY,
                       s.session.device_pub, BLIFI_KEY_LEN);
        ESP_LOGI(TAG, "handshake: keys derived, awaiting confirmation");
    } else if (msg_type == BLIFI_MSG_HS_CONFIRM) {
        if (!s.session_active || len != BLIFI_CONFIRM_LEN) {
            return;
        }
        if (esp_timer_get_time() < s.lockout_until) {
            ESP_LOGW(TAG, "handshake locked out");
            uint8_t code = BLIFI_STATUS_AUTH_FAILED;
            blifi_ble_send(BLIFI_CH_HANDSHAKE, BLIFI_MSG_HS_FAIL, &code, 1);
            return;
        }
        uint8_t expect[BLIFI_CONFIRM_LEN];
        if (blifi_crypto_confirm_app(&s.session, expect) != ESP_OK ||
            !ct_equal(expect, payload, BLIFI_CONFIRM_LEN)) {
            s.fail_count++;
            ESP_LOGW(TAG, "confirmation mismatch (%d)", s.fail_count);
            if (s.fail_count >= LOCKOUT_FAILS) {
                s.lockout_until = esp_timer_get_time() + LOCKOUT_US;
                s.fail_count = 0;
                ESP_LOGW(TAG, "locked out for 30s");
            }
            uint8_t code = BLIFI_STATUS_AUTH_FAILED;
            blifi_ble_send(BLIFI_CH_HANDSHAKE, BLIFI_MSG_HS_FAIL, &code, 1);
            return;
        }
        uint8_t confirm_dev[BLIFI_CONFIRM_LEN];
        blifi_crypto_confirm_dev(&s.session, confirm_dev);
        blifi_ble_send(BLIFI_CH_HANDSHAKE, BLIFI_MSG_HS_CONFIRM, confirm_dev, BLIFI_CONFIRM_LEN);
        s.established = true;
        s.fail_count = 0;
        ESP_LOGI(TAG, "handshake confirmed — session established");
    }
}

static void handle_scan_request(uint8_t msg_type, const uint8_t *record, size_t len)
{
    if (!s.established) {
        return;
    }
    uint8_t pt[64];
    size_t ptlen = 0;
    if (blifi_crypto_decrypt(&s.session, msg_type, record, len, pt, sizeof(pt), &ptlen) != ESP_OK) {
        return;
    }
    size_t found = 0;
    blifi_wifi_manager_scan(s_aps, sizeof(s_aps) / sizeof(s_aps[0]), &found);
    size_t n = 0;
    if (blifi_msg_scan_response_encode(s_aps, found, s_scratch, sizeof(s_scratch), &n) == ESP_OK) {
        send_encrypted(BLIFI_CH_SCAN, BLIFI_MSG_SCAN_RESPONSE, s_scratch, n);
        ESP_LOGI(TAG, "sent %u scan results", (unsigned)found);
    }
}

static void handle_credentials(uint8_t msg_type, const uint8_t *record, size_t len)
{
    if (!s.established) {
        return;
    }
    uint8_t pt[512];
    size_t ptlen = 0;
    if (blifi_crypto_decrypt(&s.session, msg_type, record, len, pt, sizeof(pt), &ptlen) != ESP_OK) {
        send_status(BLIFI_STATUS_INVALID_MESSAGE, NULL);
        return;
    }
    blifi_wifi_credentials_t creds;
    if (blifi_msg_credentials_decode(pt, ptlen, &creds) != ESP_OK) {
        send_status(BLIFI_STATUS_INVALID_MESSAGE, NULL);
        return;
    }
    ESP_LOGI(TAG, "credentials received for ssid=\"%s\"", creds.ssid);
    send_status(BLIFI_STATUS_CREDENTIALS_RECEIVED, NULL);
    s.state = ST_CONNECTING;
    blifi_wifi_manager_connect(&creds);
}

static void ble_on_message(void *ctx, blifi_ble_char_t ch, uint8_t msg_type,
                           const uint8_t *payload, size_t len)
{
    LOCK();
    switch (ch) {
    case BLIFI_CH_HANDSHAKE:   handle_handshake(msg_type, payload, len);      break;
    case BLIFI_CH_SCAN:        handle_scan_request(msg_type, payload, len);   break;
    case BLIFI_CH_CREDENTIALS: handle_credentials(msg_type, payload, len);    break;
    default: break;
    }
    UNLOCK();
}

static void ble_on_conn(void *ctx, bool connected)
{
    LOCK();
    if (connected) {
        if (blifi_crypto_session_init(&s.session) == ESP_OK) {
            s.session_active = true;
        }
        s.established = false;
        if (s.state == ST_UNPROV) {
            s.state = ST_PROV;
        }
    } else {
        blifi_crypto_session_free(&s.session);
        s.session_active = false;
        s.established = false;
    }
    UNLOCK();
}

static size_t ble_devinfo(void *ctx, uint8_t *out, size_t cap)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    size_t n = 0;
    LOCK();
    blifi_msg_device_info_encode(desc ? desc->version : "0.0.0",
                                 blifi_ble_device_name(), state_str(s.state),
                                 s.cfg.require_pop, out, cap, &n);
    UNLOCK();
    return n;
}

/* --------------------------------------------------- Wi-Fi status bridge */

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const blifi_event_data_t *e = data;
    LOCK();
    switch (id) {
    case BLIFI_EVENT_WIFI_CONNECTED:
        s.state = ST_CONNECTED;
        send_status(BLIFI_STATUS_WIFI_CONNECTED, &e->ip);
        break;
    case BLIFI_EVENT_WIFI_CONNECTING:
        send_status(BLIFI_STATUS_WIFI_CONNECTING, NULL);
        break;
    case BLIFI_EVENT_WIFI_FAILED:
        send_status(e->status, NULL);
        break;
    case BLIFI_EVENT_REPROVISIONING_TRIGGERED:
        s.state = ST_PROV;
        UNLOCK();
        blifi_start(); /* ensure advertising */
        return;
    default:
        break;
    }
    UNLOCK();
}

static void start_ble(void)
{
    if (s.ble_started) {
        return;
    }
    blifi_ble_config_t c = {
        .device_name     = s.cfg.device_name,
        .on_message      = ble_on_message,
        .on_conn         = ble_on_conn,
        .on_devinfo_read = ble_devinfo,
        .ctx             = NULL,
    };
    if (blifi_ble_start(&c) == ESP_OK) {
        s.ble_started = true;
    }
}

/* -------------------------------------------------------------- public */

esp_err_t blifi_init(const blifi_config_t *config)
{
    if (s.inited) {
        return ESP_OK;
    }
    s.cfg = config ? *config : (blifi_config_t)BLIFI_DEFAULT_CONFIG();
    s.lock = xSemaphoreCreateRecursiveMutex();
    if (!s.lock) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(blifi_wifi_manager_init(&s.cfg.wifi), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
        BLIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL, NULL), TAG, "evt reg");
    pop_load_or_create();
    s.state = ST_UNPROV;
    s.inited = true;
    return ESP_OK;
}

esp_err_t blifi_start(void)
{
    if (!s.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (blifi_is_provisioned()) {
        LOCK();
        if (s.state != ST_CONNECTED) {
            s.state = ST_CONNECTING;
        }
        UNLOCK();
        ESP_LOGI(TAG, "provisioned — connecting to Wi-Fi");
        return blifi_wifi_manager_start();
    }
    LOCK();
    s.state = ST_PROV;
    UNLOCK();
    ESP_LOGI(TAG, "unprovisioned — advertising for provisioning");
    start_ble();
    return ESP_OK;
}

esp_err_t blifi_stop(void)
{
    blifi_wifi_manager_stop();
    if (s.ble_started) {
        blifi_ble_stop();
    }
    return ESP_OK;
}

bool blifi_is_provisioned(void)
{
    return blifi_wifi_manager_has_credentials();
}

esp_err_t blifi_reset_credentials(void)
{
    blifi_wifi_manager_stop();
    esp_err_t err = blifi_wifi_manager_erase_credentials();
    LOCK();
    s.state = ST_UNPROV;
    UNLOCK();
    start_ble();
    ESP_LOGI(TAG, "credentials reset — back to provisioning");
    return err;
}

const char *blifi_get_pop(void)
{
    return s.pop;
}
