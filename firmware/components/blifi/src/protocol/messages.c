/**
 * @file messages.c
 * @brief JSON payload codecs (docs/protocol-spec.md §6). Self-contained minimal
 *        JSON - the wire format is JSON; no external parser dependency.
 */
#include "messages.h"
#include "frame.h" /* BLIFI_PROTO_VERSION */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "blifi_msg";

/* ------------------------------------------------------------ JSON builder */

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
    bool   ok;
} jbuf_t;

static void jb_putc(jbuf_t *j, char c)
{
    if (j->len + 1 < j->cap) {
        j->buf[j->len++] = c;
    } else {
        j->ok = false;
    }
}

static void jb_raw(jbuf_t *j, const char *s)
{
    while (*s) {
        jb_putc(j, *s++);
    }
}

/* Append a JSON string literal with escaping. */
static void jb_str(jbuf_t *j, const char *s)
{
    jb_putc(j, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  jb_raw(j, "\\\""); break;
        case '\\': jb_raw(j, "\\\\"); break;
        case '\n': jb_raw(j, "\\n");  break;
        case '\r': jb_raw(j, "\\r");  break;
        case '\t': jb_raw(j, "\\t");  break;
        case '\b': jb_raw(j, "\\b");  break;
        case '\f': jb_raw(j, "\\f");  break;
        default:
            if (*p < 0x20) {
                char t[8];
                snprintf(t, sizeof(t), "\\u%04x", *p);
                jb_raw(j, t);
            } else {
                jb_putc(j, (char)*p); /* UTF-8 bytes pass through */
            }
        }
    }
    jb_putc(j, '"');
}

static void jb_key(jbuf_t *j, const char *k, bool *first)
{
    if (!*first) {
        jb_putc(j, ',');
    }
    *first = false;
    jb_str(j, k);
    jb_putc(j, ':');
}

static void jb_kv_str(jbuf_t *j, const char *k, const char *v, bool *first)
{
    jb_key(j, k, first);
    jb_str(j, v);
}

static void jb_kv_int(jbuf_t *j, const char *k, long v, bool *first)
{
    jb_key(j, k, first);
    char t[16];
    snprintf(t, sizeof(t), "%ld", v);
    jb_raw(j, t);
}

static void jb_kv_bool(jbuf_t *j, const char *k, bool v, bool *first)
{
    jb_key(j, k, first);
    jb_raw(j, v ? "true" : "false");
}

static esp_err_t jb_finish(jbuf_t *j, size_t *out_len)
{
    if (!j->ok || j->len + 1 > j->cap) {
        return ESP_ERR_INVALID_SIZE;
    }
    j->buf[j->len] = '\0';
    if (out_len) {
        *out_len = j->len;
    }
    return ESP_OK;
}

/* ------------------------------------------------------------- encoders */

esp_err_t blifi_msg_device_info_encode(const char *fw, const char *name,
                                       const char *state, bool pop_required,
                                       uint8_t *out, size_t cap, size_t *out_len)
{
    jbuf_t j = { .buf = (char *)out, .cap = cap, .ok = true };
    bool first = true;
    jb_putc(&j, '{');
    jb_kv_int(&j, "proto", BLIFI_PROTO_VERSION, &first);
    jb_kv_str(&j, "fw", fw ? fw : "0.0.0", &first);
    jb_kv_str(&j, "name", name ? name : "blifi", &first);
    jb_kv_str(&j, "state", state ? state : "unprovisioned", &first);
    jb_kv_bool(&j, "pop_required", pop_required, &first);
    jb_putc(&j, '}');
    return jb_finish(&j, out_len);
}

esp_err_t blifi_msg_scan_response_encode(const blifi_wifi_ap_t *aps, size_t n,
                                         uint8_t *out, size_t cap, size_t *out_len)
{
    jbuf_t j = { .buf = (char *)out, .cap = cap, .ok = true };
    jb_raw(&j, "{\"networks\":[");
    for (size_t i = 0; i < n; i++) {
        if (i) {
            jb_putc(&j, ',');
        }
        bool first = true;
        jb_putc(&j, '{');
        jb_kv_str(&j, "ssid", aps[i].ssid, &first);
        jb_kv_int(&j, "rssi", aps[i].rssi, &first);
        jb_kv_int(&j, "auth", aps[i].authmode, &first);
        jb_kv_int(&j, "channel", aps[i].channel, &first);
        jb_kv_bool(&j, "hidden", aps[i].hidden, &first);
        jb_putc(&j, '}');
    }
    jb_raw(&j, "]}");
    return jb_finish(&j, out_len);
}

esp_err_t blifi_msg_status_encode(blifi_status_t code, const char *detail,
                                  const esp_ip4_addr_t *ip,
                                  uint8_t *out, size_t cap, size_t *out_len)
{
    jbuf_t j = { .buf = (char *)out, .cap = cap, .ok = true };
    bool first = true;
    jb_putc(&j, '{');
    jb_kv_int(&j, "code", code, &first);
    if (detail) {
        jb_kv_str(&j, "detail", detail, &first);
    }
    if (ip) {
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(ip));
        jb_kv_str(&j, "ip", buf, &first);
    }
    jb_putc(&j, '}');
    return jb_finish(&j, out_len);
}

/* --------------------------------------------------------- JSON scanner */

static const char *skip_ws(const char *p, const char *e)
{
    while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

static int hex4(const char *p)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9')      v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

/* Parse a JSON string at `p` (points at opening quote) into `out` (UTF-8, NUL-
 * terminated). Returns the pointer past the closing quote, or NULL on error. */
static const char *parse_string(const char *p, const char *e, char *out, size_t cap)
{
    if (p >= e || *p != '"') {
        return NULL;
    }
    p++;
    size_t n = 0;
    while (p < e && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            if (p >= e) return NULL;
            char x = *p++;
            switch (x) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'u': {
                if (e - p < 4) return NULL;
                int cp = hex4(p);
                if (cp < 0) return NULL;
                p += 4;
                if (cp < 0x80) {
                    if (n + 1 < cap) out[n++] = (char)cp;
                } else if (cp < 0x800) {
                    if (n + 2 < cap) {
                        out[n++] = (char)(0xC0 | (cp >> 6));
                        out[n++] = (char)(0x80 | (cp & 0x3f));
                    }
                } else {
                    if (n + 3 < cap) {
                        out[n++] = (char)(0xE0 | (cp >> 12));
                        out[n++] = (char)(0x80 | ((cp >> 6) & 0x3f));
                        out[n++] = (char)(0x80 | (cp & 0x3f));
                    }
                }
                continue;
            }
            default: return NULL;
            }
        }
        if (n + 1 < cap) {
            out[n++] = c;
        } else {
            return NULL;
        }
    }
    if (p >= e) return NULL;
    out[n] = '\0';
    return p + 1; /* past closing quote */
}

/* Advance past one JSON value (string/number/bool/null/object/array). */
static const char *skip_value(const char *p, const char *e)
{
    p = skip_ws(p, e);
    if (p >= e) return NULL;
    if (*p == '"') {
        char tmp[128];
        return parse_string(p, e, tmp, sizeof(tmp));
    }
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (p < e) {
            if (*p == '"') {
                char tmp[128];
                const char *np = parse_string(p, e, tmp, sizeof(tmp));
                if (!np) return NULL;
                p = np;
                continue;
            }
            if (*p == open) depth++;
            else if (*p == close && --depth == 0) return p + 1;
            p++;
        }
        return NULL;
    }
    /* number / true / false / null */
    while (p < e && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        p++;
    }
    return p;
}

/* Callback for each top-level member. `vstart` points at the (ws-skipped) value. */
typedef void (*member_cb)(void *ctx, const char *key, const char *vstart, const char *vend);

static esp_err_t json_walk(const char *json, size_t len, member_cb cb, void *ctx)
{
    const char *p = json, *e = json + len;
    p = skip_ws(p, e);
    if (p >= e || *p != '{') return ESP_ERR_INVALID_ARG;
    p++;
    p = skip_ws(p, e);
    if (p < e && *p == '}') return ESP_OK; /* empty object */

    while (p < e) {
        char key[24];
        p = parse_string(p, e, key, sizeof(key));
        if (!p) return ESP_ERR_INVALID_ARG;
        p = skip_ws(p, e);
        if (p >= e || *p != ':') return ESP_ERR_INVALID_ARG;
        p = skip_ws(p + 1, e);
        const char *vstart = p;
        const char *vend = skip_value(p, e);
        if (!vend) return ESP_ERR_INVALID_ARG;
        cb(ctx, key, vstart, vend);
        p = skip_ws(vend, e);
        if (p >= e) return ESP_ERR_INVALID_ARG;
        if (*p == ',') { p = skip_ws(p + 1, e); continue; }
        if (*p == '}') return ESP_OK;
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_ERR_INVALID_ARG;
}

/* ------------------------------------------------------------- decoders */

typedef struct {
    bool *refresh;
} scanreq_ctx_t;

static void scanreq_member(void *ctx, const char *key, const char *vstart, const char *vend)
{
    scanreq_ctx_t *c = ctx;
    if (c->refresh && strcmp(key, "refresh") == 0) {
        *c->refresh = (vend - vstart >= 4 && strncmp(vstart, "true", 4) == 0);
    }
}

esp_err_t blifi_msg_scan_request_decode(const uint8_t *json, size_t len, bool *refresh)
{
    if (refresh) {
        *refresh = false;
    }
    scanreq_ctx_t ctx = { .refresh = refresh };
    return json_walk((const char *)json, len, scanreq_member, &ctx);
}

typedef struct {
    blifi_wifi_credentials_t *creds;
    bool have_bssid;
    bool have_chan;
} creds_ctx_t;

static void creds_member(void *ctx, const char *key, const char *vstart, const char *vend)
{
    creds_ctx_t *c = ctx;
    const char *e = vend;
    if (strcmp(key, "ssid") == 0) {
        parse_string(vstart, e, c->creds->ssid, sizeof(c->creds->ssid));
    } else if (strcmp(key, "password") == 0) {
        parse_string(vstart, e, c->creds->password, sizeof(c->creds->password));
    } else if (strcmp(key, "bssid") == 0) {
        char b[24];
        if (parse_string(vstart, e, b, sizeof(b))) {
            unsigned v[6];
            if (sscanf(b, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
                for (int i = 0; i < 6; i++) {
                    c->creds->bssid[i] = (uint8_t)v[i];
                }
                c->have_bssid = true;
            }
        }
    } else if (strcmp(key, "channel") == 0) {
        c->creds->channel = (uint8_t)strtol(vstart, NULL, 10);
        c->have_chan = true;
    }
}

esp_err_t blifi_msg_credentials_decode(const uint8_t *json, size_t len,
                                       blifi_wifi_credentials_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    creds_ctx_t ctx = { .creds = out };
    esp_err_t err = json_walk((const char *)json, len, creds_member, &ctx);
    if (err != ESP_OK) {
        return err;
    }
    if (out->ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    out->bssid_set = ctx.have_bssid && ctx.have_chan;
    ESP_LOGD(TAG, "credentials for ssid=\"%s\"", out->ssid);
    return ESP_OK;
}
