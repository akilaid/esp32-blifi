/**
 * @file ble_transport.c
 * @brief NimBLE GATT server + framing for the blifi provisioning service.
 */
#include "ble_transport.h"
#include "frame.h"

#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "blifi_ble";

/* 128-bit UUIDs (protocol-spec §2), byte 12 selects the characteristic.
 * NimBLE stores 128-bit UUIDs little-endian (reverse of the string form). */
#define BLIFI_UUID128(sel) BLE_UUID128_INIT( \
    0x20,0x9b,0x4c,0x7a,0x8f,0x1e,0x2d,0x9c,0x7a,0x4b,0x3e,0x5f,(sel),0x00,0x1a,0x6b)

static const ble_uuid128_t svc_uuid  = BLIFI_UUID128(0x01);
static const ble_uuid128_t di_uuid   = BLIFI_UUID128(0x02);
static const ble_uuid128_t hs_uuid   = BLIFI_UUID128(0x03);
static const ble_uuid128_t scan_uuid = BLIFI_UUID128(0x04);
static const ble_uuid128_t cred_uuid = BLIFI_UUID128(0x05);
static const ble_uuid128_t stat_uuid = BLIFI_UUID128(0x06);

static blifi_ble_config_t s_cfg;
static char               s_name[32];
static uint8_t            s_own_addr_type;
static uint16_t           s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t           s_handles[BLIFI_CH_COUNT];
static blifi_reasm_t      s_reasm[BLIFI_CH_COUNT];

static void start_advertising(void);

/* ------------------------------------------------------------- GATT access */

static void handle_inbound(blifi_ble_char_t ch, const uint8_t *frame, size_t len)
{
    if (ch >= BLIFI_CH_COUNT) {
        return;
    }
    blifi_reasm_status_t st = blifi_reasm_feed(&s_reasm[ch], frame, len);
    if (st == BLIFI_REASM_COMPLETE && s_cfg.on_message) {
        s_cfg.on_message(s_cfg.ctx, ch, s_reasm[ch].msg_type,
                         s_reasm[ch].buf, s_reasm[ch].total_len);
    }
}

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    blifi_ble_char_t ch = (blifi_ble_char_t)(uintptr_t)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (ch == BLIFI_CH_DEVICE_INFO && s_cfg.on_devinfo_read) {
            uint8_t buf[256];
            size_t n = s_cfg.on_devinfo_read(s_cfg.ctx, buf, sizeof(buf));
            return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t frame[BLIFI_FRAME_HEADER_LEN + 512];
        uint16_t plen = OS_MBUF_PKTLEN(ctxt->om);
        if (plen > sizeof(frame)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint16_t copied = 0;
        ble_hs_mbuf_to_flat(ctxt->om, frame, sizeof(frame), &copied);
        handle_inbound(ch, frame, copied);
        return 0;
    }
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &di_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)BLIFI_CH_DEVICE_INFO,
                .flags = BLE_GATT_CHR_F_READ,
                .val_handle = &s_handles[BLIFI_CH_DEVICE_INFO],
            },
            {
                .uuid = &hs_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)BLIFI_CH_HANDSHAKE,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handles[BLIFI_CH_HANDSHAKE],
            },
            {
                .uuid = &scan_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)BLIFI_CH_SCAN,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handles[BLIFI_CH_SCAN],
            },
            {
                .uuid = &cred_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)BLIFI_CH_CREDENTIALS,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_handles[BLIFI_CH_CREDENTIALS],
            },
            {
                .uuid = &stat_uuid.u,
                .access_cb = gatt_access,
                .arg = (void *)(uintptr_t)BLIFI_CH_STATUS,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handles[BLIFI_CH_STATUS],
            },
            { 0 },
        },
    },
    { 0 },
};

/* ------------------------------------------------------------- GAP events */

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn = event->connect.conn_handle;
            for (int i = 0; i < BLIFI_CH_COUNT; i++) {
                blifi_reasm_reset(&s_reasm[i]);
            }
            ESP_LOGI(TAG, "central connected");
            if (s_cfg.on_conn) {
                s_cfg.on_conn(s_cfg.ctx, true);
            }
        } else {
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "central disconnected (reason 0x%02x)", event->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        if (s_cfg.on_conn) {
            s_cfg.on_conn(s_cfg.ctx, false);
        }
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_name;
    fields.name_len = strlen(s_name);
    fields.name_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed");
        return;
    }

    /* Advertise the 128-bit service UUID in the scan response. */
    struct ble_hs_adv_fields rsp = {0};
    rsp.uuids128 = (ble_uuid128_t *)&svc_uuid;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                               &adv_params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "advertising as \"%s\"", s_name);
    }
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    if (ble_hs_id_infer_auto(0, &s_own_addr_type) != 0) {
        ESP_LOGE(TAG, "no BLE address");
        return;
    }
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: %d", reason);
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ------------------------------------------------------------- public API */

esp_err_t blifi_ble_start(const blifi_ble_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *config;

    if (config->device_name && config->device_name[0]) {
        strlcpy(s_name, config->device_name, sizeof(s_name));
    } else {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(s_name, sizeof(s_name), "blifi-%02X%02X", mac[4], mac[5]);
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc == 0) rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt register failed: %d", rc);
        return ESP_FAIL;
    }
    ble_svc_gap_device_name_set(s_name);

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BLE started");
    return ESP_OK;
}

esp_err_t blifi_ble_stop(void)
{
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    ble_gap_adv_stop();
    return ESP_OK;
}

esp_err_t blifi_ble_shutdown(void)
{
    /* Stop soliciting and drop the link before tearing the host down. The
     * DISCONNECT event this may raise re-advertises briefly on the host task;
     * nimble_port_stop() below then halts it - so callers must not hold any lock
     * the host task needs (it is joined here). */
    ble_gap_adv_stop();
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    }

    int rc = nimble_port_stop();   /* signals nimble_port_run() to return + joins host_task */
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_stop failed: %d", rc);
        return ESP_FAIL;
    }
    nimble_port_deinit();          /* free the host so blifi_ble_start() can re-init */

    /* Reset transport state so a later blifi_ble_start() is a clean slate. */
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    memset(s_handles, 0, sizeof(s_handles));
    for (int i = 0; i < BLIFI_CH_COUNT; i++) {
        blifi_reasm_reset(&s_reasm[i]);
    }
    ESP_LOGI(TAG, "BLE stack shut down (host deinitialised)");
    return ESP_OK;
}

typedef struct {
    uint16_t val_handle;
    esp_err_t err;
} emit_ctx_t;

static esp_err_t emit_notify(void *ctx, const uint8_t *frame, size_t len)
{
    emit_ctx_t *e = ctx;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(frame, (uint16_t)len);
    if (!om) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn, e->val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t blifi_ble_send(blifi_ble_char_t ch, uint8_t msg_type,
                         const uint8_t *payload, size_t len)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE || ch >= BLIFI_CH_COUNT) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t mtu = ble_att_mtu(s_conn);
    if (mtu < 23) {
        mtu = 23;
    }
    size_t max_chunk = mtu - 3 - BLIFI_FRAME_HEADER_LEN;
    emit_ctx_t e = { .val_handle = s_handles[ch], .err = ESP_OK };
    return blifi_frame_send(msg_type, payload, len, max_chunk, emit_notify, &e);
}

bool blifi_ble_is_connected(void)
{
    return s_conn != BLE_HS_CONN_HANDLE_NONE;
}

const char *blifi_ble_device_name(void)
{
    return s_name;
}
