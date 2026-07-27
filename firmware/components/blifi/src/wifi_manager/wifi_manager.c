/**
 * @file wifi_manager.c
 * @brief Wi-Fi station manager: scan, connect, NVS persistence, retry/backoff.
 *
 * Event-driven: Wi-Fi/IP events run the connection state machine on the default
 * event loop task; reconnect backoff and a per-attempt connect timeout are
 * one-shot esp_timers. Status changes are published on ::BLIFI_EVENT.
 */
#include "blifi_wifi_manager.h"
#include "blifi.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"

ESP_EVENT_DEFINE_BASE(BLIFI_EVENT);

static const char *TAG = "blifi_wifi";

#define BLIFI_NVS_NAMESPACE "blifi"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"
#define NVS_KEY_BSSID "bssid"
#define NVS_KEY_CHAN  "chan"

/** NVS partition holding credentials: the configured one, or the default "nvs". */
static const char *creds_partition(void);

typedef struct {
    bool                        initialized;
    blifi_wifi_manager_config_t cfg;
    esp_netif_t                *netif;

    blifi_status_t              status;
    blifi_wifi_credentials_t    creds;
    bool                        active;     /*!< connecting/connected, retries armed */
    bool                        got_ip;     /*!< currently has an IP */
    bool                        timed_out;  /*!< current attempt hit connect timeout */
    uint8_t                     retries;    /*!< consecutive failed attempts */
    uint32_t                    backoff_ms; /*!< next reconnect delay */

    esp_timer_handle_t          backoff_timer;
    esp_timer_handle_t          conn_timer;
} blifi_wifi_ctx_t;

static blifi_wifi_ctx_t s;

/* ------------------------------------------------------------------ helpers */

static const char *creds_partition(void)
{
    return (s.cfg.nvs_partition && s.cfg.nvs_partition[0])
               ? s.cfg.nvs_partition
               : NVS_DEFAULT_PART_NAME;
}

const char *blifi_status_str(blifi_status_t status)
{
    switch (status) {
    case BLIFI_STATUS_IDLE:                  return "IDLE";
    case BLIFI_STATUS_HANDSHAKE_IN_PROGRESS: return "HANDSHAKE_IN_PROGRESS";
    case BLIFI_STATUS_HANDSHAKE_OK:          return "HANDSHAKE_OK";
    case BLIFI_STATUS_CREDENTIALS_RECEIVED:  return "CREDENTIALS_RECEIVED";
    case BLIFI_STATUS_WIFI_CONNECTING:       return "WIFI_CONNECTING";
    case BLIFI_STATUS_WIFI_CONNECTED:        return "WIFI_CONNECTED";
    case BLIFI_STATUS_AUTH_FAILED:           return "AUTH_FAILED";
    case BLIFI_STATUS_WIFI_NOT_FOUND:        return "WIFI_NOT_FOUND";
    case BLIFI_STATUS_WIFI_AUTH_ERROR:       return "WIFI_AUTH_ERROR";
    case BLIFI_STATUS_WIFI_TIMEOUT:          return "WIFI_TIMEOUT";
    case BLIFI_STATUS_WIFI_DISCONNECTED:     return "WIFI_DISCONNECTED";
    case BLIFI_STATUS_PROV_TIMEOUT:          return "PROV_TIMEOUT";
    case BLIFI_STATUS_INVALID_MESSAGE:       return "INVALID_MESSAGE";
    case BLIFI_STATUS_INTERNAL_ERROR:        return "INTERNAL_ERROR";
    default:                                 return "UNKNOWN";
    }
}

/** Map a wifi_err_reason_t to a blifi status code. */
static blifi_status_t reason_to_status(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return BLIFI_STATUS_WIFI_NOT_FOUND;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
        return BLIFI_STATUS_WIFI_AUTH_ERROR;
    case WIFI_REASON_CONNECTION_FAIL:
    case WIFI_REASON_BEACON_TIMEOUT:
        return BLIFI_STATUS_WIFI_TIMEOUT;
    default:
        return BLIFI_STATUS_WIFI_DISCONNECTED;
    }
}

static bool is_auth_reason(uint8_t reason)
{
    return reason_to_status(reason) == BLIFI_STATUS_WIFI_AUTH_ERROR;
}

static void post_event(blifi_event_id_t id, blifi_status_t status,
                       int wifi_reason, const esp_ip4_addr_t *ip)
{
    s.status = status;
    blifi_event_data_t data = { .status = status, .wifi_reason = wifi_reason };
    if (ip) {
        data.ip = *ip;
    }
    esp_err_t err = esp_event_post(BLIFI_EVENT, id, &data, sizeof(data), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_event_post(%d) failed: %s", id, esp_err_to_name(err));
    }
}

/* ------------------------------------------------------ state machine steps */

/** Begin one connection attempt: arm the connect-timeout and call connect. */
static void start_attempt(void)
{
    s.timed_out = false;
    post_event(BLIFI_EVENT_WIFI_CONNECTING, BLIFI_STATUS_WIFI_CONNECTING, 0, NULL);

    esp_timer_stop(s.conn_timer);
    esp_timer_start_once(s.conn_timer, (uint64_t)s.cfg.connect_timeout_ms * 1000);

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(err));
    }
}

/** Schedule the next attempt after the current backoff, then grow the backoff. */
static void schedule_retry(void)
{
    ESP_LOGI(TAG, "reconnect in %" PRIu32 " ms (attempt %u/%u)",
             s.backoff_ms, (unsigned)(s.retries + 1), (unsigned)s.cfg.max_retries);
    esp_timer_stop(s.backoff_timer);
    esp_timer_start_once(s.backoff_timer, (uint64_t)s.backoff_ms * 1000);

    uint64_t next = (uint64_t)s.backoff_ms * s.cfg.backoff_multiplier;
    s.backoff_ms = (next > s.cfg.backoff_max_ms) ? s.cfg.backoff_max_ms : (uint32_t)next;
}

/** Give up: publish the failure. The provisioning_manager's WIFI_FAILED handler
 *  returns the device to the provisioning state. */
static void enter_failed(blifi_status_t status, int reason)
{
    s.active = false;
    esp_timer_stop(s.backoff_timer);
    esp_timer_stop(s.conn_timer);
    ESP_LOGW(TAG, "giving up: %s", blifi_status_str(status));
    post_event(BLIFI_EVENT_WIFI_FAILED, status, reason, NULL);
}

static void handle_disconnected(uint8_t reason)
{
    esp_timer_stop(s.conn_timer);
    if (!s.active) {
        return; /* stop() requested, or a stray event */
    }

    blifi_status_t st = s.timed_out ? BLIFI_STATUS_WIFI_TIMEOUT : reason_to_status(reason);

    if (s.got_ip) {
        /* Was connected - a drop. Reset the retry budget and reconnect. */
        ESP_LOGW(TAG, "connection dropped (reason %u), reconnecting", reason);
        s.got_ip = false;
        s.retries = 0;
        s.backoff_ms = s.cfg.backoff_base_ms;
        s.status = st;
        schedule_retry();
        return;
    }

    /* A failed attempt. */
    if (s.cfg.fast_fail_on_auth_error && !s.timed_out && is_auth_reason(reason)) {
        enter_failed(BLIFI_STATUS_WIFI_AUTH_ERROR, reason);
        return;
    }

    s.retries++;
    if (s.retries >= s.cfg.max_retries) {
        enter_failed(st, reason);
        return;
    }
    s.status = st;
    schedule_retry();
}

/* ------------------------------------------------------------ event handlers */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "associated, waiting for IP");
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *e = data;
        handle_disconnected(e ? e->reason : 0);
        break;
    }
    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base,
                             int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const ip_event_got_ip_t *e = data;
    esp_timer_stop(s.conn_timer);
    esp_timer_stop(s.backoff_timer);
    s.got_ip = true;
    s.retries = 0;
    s.backoff_ms = s.cfg.backoff_base_ms;
    ESP_LOGI(TAG, "connected, ip=" IPSTR, IP2STR(&e->ip_info.ip));
    post_event(BLIFI_EVENT_WIFI_CONNECTED, BLIFI_STATUS_WIFI_CONNECTED, 0, &e->ip_info.ip);
}

static void backoff_timer_cb(void *arg)
{
    if (s.active && !s.got_ip) {
        start_attempt();
    }
}

static void conn_timer_cb(void *arg)
{
    if (s.active && !s.got_ip) {
        ESP_LOGW(TAG, "connect attempt timed out");
        s.timed_out = true;
        esp_wifi_disconnect(); /* yields STA_DISCONNECTED, handled above */
    }
}

/* -------------------------------------------------------------------- public */

esp_err_t blifi_wifi_manager_init(const blifi_wifi_manager_config_t *config)
{
    if (s.initialized) {
        return ESP_OK;
    }
    s.cfg = config ? *config : (blifi_wifi_manager_config_t)BLIFI_WIFI_MANAGER_DEFAULT_CONFIG();
    s.status = BLIFI_STATUS_IDLE;
    s.backoff_ms = s.cfg.backoff_base_ms;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif_init");

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { /* already created is fine */
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return err;
    }

    s.netif = esp_netif_create_default_wifi_sta();
    if (!s.netif) {
        return ESP_FAIL;
    }

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi_init");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL), TAG, "reg wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL, NULL), TAG, "reg ip");

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi_start");

    const esp_timer_create_args_t backoff_args = {
        .callback = backoff_timer_cb, .name = "blifi_backoff" };
    const esp_timer_create_args_t conn_args = {
        .callback = conn_timer_cb, .name = "blifi_conn" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&backoff_args, &s.backoff_timer), TAG, "backoff_timer");
    ESP_RETURN_ON_ERROR(esp_timer_create(&conn_args, &s.conn_timer), TAG, "conn_timer");

    s.initialized = true;
    ESP_LOGI(TAG, "initialised (max_retries=%u, timeout=%" PRIu32 "ms)",
             (unsigned)s.cfg.max_retries, s.cfg.connect_timeout_ms);
    return ESP_OK;
}

esp_err_t blifi_wifi_manager_scan(blifi_wifi_ap_t *out, size_t max, size_t *found)
{
    if (found) {
        *found = 0;
    }
    if (!s.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t scan = { .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_ACTIVE };
    ESP_RETURN_ON_ERROR(esp_wifi_scan_start(&scan, true), TAG, "scan_start");

    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);
    if (num == 0 || max == 0 || out == NULL) {
        return ESP_OK;
    }

    uint16_t want = (num < max) ? num : (uint16_t)max;
    wifi_ap_record_t *recs = calloc(want, sizeof(*recs));
    if (!recs) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_wifi_scan_get_ap_records(&want, recs);
    if (err == ESP_OK) {
        for (uint16_t i = 0; i < want; i++) {
            blifi_wifi_ap_t *ap = &out[i];
            memcpy(ap->ssid, recs[i].ssid, sizeof(ap->ssid) - 1);
            ap->ssid[sizeof(ap->ssid) - 1] = '\0';
            memcpy(ap->bssid, recs[i].bssid, sizeof(ap->bssid));
            ap->rssi = recs[i].rssi;
            ap->authmode = recs[i].authmode;
            ap->channel = recs[i].primary;
            ap->hidden = (recs[i].ssid[0] == '\0');
        }
        if (found) {
            *found = want;
        }
    }
    free(recs);
    return err;
}

esp_err_t blifi_wifi_manager_save_credentials(const blifi_wifi_credentials_t *creds)
{
    if (!creds || creds->ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(creds_partition(), BLIFI_NVS_NAMESPACE,
                                                NVS_READWRITE, &h), TAG, "nvs_open");

    esp_err_t err = nvs_set_str(h, NVS_KEY_SSID, creds->ssid);
    if (err == ESP_OK) err = nvs_set_str(h, NVS_KEY_PASS, creds->password);
    if (err == ESP_OK) {
        if (creds->bssid_set) {
            err = nvs_set_blob(h, NVS_KEY_BSSID, creds->bssid, sizeof(creds->bssid));
            if (err == ESP_OK) err = nvs_set_u8(h, NVS_KEY_CHAN, creds->channel);
        } else {
            nvs_erase_key(h, NVS_KEY_BSSID); /* ignore NOT_FOUND */
            nvs_erase_key(h, NVS_KEY_CHAN);
        }
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        s.creds = *creds;
        ESP_LOGI(TAG, "credentials saved for ssid=\"%s\"", creds->ssid);
    }
    return err;
}

esp_err_t blifi_wifi_manager_load_credentials(blifi_wifi_credentials_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(creds_partition(), BLIFI_NVS_NAMESPACE,
                                            NVS_READONLY, &h);
    if (err != ESP_OK) {
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    blifi_wifi_credentials_t c = {0};
    size_t len = sizeof(c.ssid);
    err = nvs_get_str(h, NVS_KEY_SSID, c.ssid, &len);
    if (err == ESP_OK) {
        len = sizeof(c.password);
        err = nvs_get_str(h, NVS_KEY_PASS, c.password, &len);
    }
    if (err == ESP_OK) {
        size_t blen = sizeof(c.bssid);
        if (nvs_get_blob(h, NVS_KEY_BSSID, c.bssid, &blen) == ESP_OK && blen == sizeof(c.bssid) &&
            nvs_get_u8(h, NVS_KEY_CHAN, &c.channel) == ESP_OK) {
            c.bssid_set = true;
        }
    }
    nvs_close(h);

    if (err == ESP_OK) {
        *out = c;
    }
    return err;
}

bool blifi_wifi_manager_has_credentials(void)
{
    blifi_wifi_credentials_t c;
    return blifi_wifi_manager_load_credentials(&c) == ESP_OK;
}

esp_err_t blifi_wifi_manager_erase_credentials(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(creds_partition(), BLIFI_NVS_NAMESPACE,
                                            NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_open");
    nvs_erase_key(h, NVS_KEY_SSID);
    nvs_erase_key(h, NVS_KEY_PASS);
    nvs_erase_key(h, NVS_KEY_BSSID);
    nvs_erase_key(h, NVS_KEY_CHAN);
    err = nvs_commit(h);
    nvs_close(h);
    memset(&s.creds, 0, sizeof(s.creds));
    ESP_LOGI(TAG, "credentials erased");
    return err;
}

esp_err_t blifi_wifi_manager_connect(const blifi_wifi_credentials_t *creds)
{
    if (!s.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (creds) {
        ESP_RETURN_ON_ERROR(blifi_wifi_manager_save_credentials(creds), TAG, "save");
        s.creds = *creds;
    } else {
        ESP_RETURN_ON_ERROR(blifi_wifi_manager_load_credentials(&s.creds), TAG, "load");
    }

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, s.creds.ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s.creds.password, sizeof(wc.sta.password));
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN; /* accept any auth mode */
    wc.sta.pmf_cfg.capable = true;              /* WPA3 compatible */
    if (s.creds.bssid_set) {
        wc.sta.bssid_set = true;
        memcpy(wc.sta.bssid, s.creds.bssid, sizeof(wc.sta.bssid));
        wc.sta.channel = s.creds.channel;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wc), TAG, "set_config");

    s.active = true;
    s.got_ip = false;
    s.retries = 0;
    s.backoff_ms = s.cfg.backoff_base_ms;
    ESP_LOGI(TAG, "connecting to \"%s\"", s.creds.ssid);
    start_attempt();
    return ESP_OK;
}

esp_err_t blifi_wifi_manager_start(void)
{
    if (!s.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!blifi_wifi_manager_has_credentials()) {
        ESP_LOGI(TAG, "no stored credentials; staying idle");
        return ESP_OK;
    }
    return blifi_wifi_manager_connect(NULL);
}

esp_err_t blifi_wifi_manager_stop(void)
{
    if (!s.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s.active = false;
    s.got_ip = false;
    esp_timer_stop(s.backoff_timer);
    esp_timer_stop(s.conn_timer);
    esp_wifi_disconnect();
    s.status = BLIFI_STATUS_IDLE;
    ESP_LOGI(TAG, "stopped");
    return ESP_OK;
}

blifi_status_t blifi_wifi_manager_status(void)
{
    return s.status;
}

esp_err_t blifi_wifi_manager_get_ip(esp_netif_ip_info_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s.status != BLIFI_STATUS_WIFI_CONNECTED || !s.netif) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_netif_get_ip_info(s.netif, out);
}
