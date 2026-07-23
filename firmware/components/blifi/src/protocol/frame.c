#include "frame.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "blifi_frame";

static inline void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static inline uint16_t get_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

esp_err_t blifi_frame_pack(const blifi_frame_header_t *hdr, const uint8_t *chunk,
                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!hdr || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t total = BLIFI_FRAME_HEADER_LEN + hdr->chunk_len;
    if (out_cap < total) {
        return ESP_ERR_INVALID_SIZE;
    }
    out[0] = hdr->version;
    out[1] = hdr->msg_type;
    put_be16(&out[2], hdr->seq);
    put_be16(&out[4], hdr->total_len);
    put_be16(&out[6], hdr->chunk_len);
    if (hdr->chunk_len && chunk) {
        memcpy(&out[BLIFI_FRAME_HEADER_LEN], chunk, hdr->chunk_len);
    }
    if (out_len) {
        *out_len = total;
    }
    return ESP_OK;
}

esp_err_t blifi_frame_send(uint8_t msg_type, const uint8_t *payload, size_t len,
                           size_t max_chunk, blifi_frame_emit_fn emit, void *ctx)
{
    if (!emit || len > 0xffff || max_chunk == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t frame[BLIFI_FRAME_HEADER_LEN + 512];
    if (max_chunk > sizeof(frame) - BLIFI_FRAME_HEADER_LEN) {
        max_chunk = sizeof(frame) - BLIFI_FRAME_HEADER_LEN;
    }

    size_t offset = 0;
    uint16_t seq = 0;
    do {
        size_t chunk = len - offset;
        if (chunk > max_chunk) {
            chunk = max_chunk;
        }
        blifi_frame_header_t hdr = {
            .version   = BLIFI_PROTO_VERSION,
            .msg_type  = msg_type,
            .seq       = seq,
            .total_len = (uint16_t)len,
            .chunk_len = (uint16_t)chunk,
        };
        size_t flen = 0;
        esp_err_t err = blifi_frame_pack(&hdr, payload ? payload + offset : NULL,
                                         frame, sizeof(frame), &flen);
        if (err != ESP_OK) {
            return err;
        }
        err = emit(ctx, frame, flen);
        if (err != ESP_OK) {
            return err;
        }
        offset += chunk;
        seq++;
    } while (offset < len);

    return ESP_OK;
}

void blifi_reasm_reset(blifi_reasm_t *r)
{
    if (r) {
        r->active = false;
        r->received = 0;
        r->next_seq = 0;
        r->total_len = 0;
    }
}

blifi_reasm_status_t blifi_reasm_feed(blifi_reasm_t *r, const uint8_t *frame, size_t len)
{
    if (!r || !frame || len < BLIFI_FRAME_HEADER_LEN) {
        return BLIFI_REASM_ERROR;
    }
    blifi_frame_header_t h = {
        .version   = frame[0],
        .msg_type  = frame[1],
        .seq       = get_be16(&frame[2]),
        .total_len = get_be16(&frame[4]),
        .chunk_len = get_be16(&frame[6]),
    };
    const uint8_t *chunk = &frame[BLIFI_FRAME_HEADER_LEN];

    if (h.version != BLIFI_PROTO_VERSION) {
        ESP_LOGW(TAG, "bad version 0x%02x", h.version);
        blifi_reasm_reset(r);
        return BLIFI_REASM_ERROR;
    }
    if (len - BLIFI_FRAME_HEADER_LEN < h.chunk_len) {
        ESP_LOGW(TAG, "short frame: have %u, chunk_len %u",
                 (unsigned)(len - BLIFI_FRAME_HEADER_LEN), h.chunk_len);
        blifi_reasm_reset(r);
        return BLIFI_REASM_ERROR;
    }

    if (h.seq == 0) {
        /* First frame of a message. */
        if (h.total_len > BLIFI_MAX_MESSAGE_LEN) {
            ESP_LOGW(TAG, "message too large: %u", h.total_len);
            blifi_reasm_reset(r);
            return BLIFI_REASM_ERROR;
        }
        r->active = true;
        r->msg_type = h.msg_type;
        r->total_len = h.total_len;
        r->received = 0;
        r->next_seq = 0;
    } else if (!r->active || h.seq != r->next_seq || h.msg_type != r->msg_type) {
        ESP_LOGW(TAG, "unexpected seq %u (want %u)", h.seq, r->next_seq);
        blifi_reasm_reset(r);
        return BLIFI_REASM_ERROR;
    }

    if ((size_t)r->received + h.chunk_len > r->total_len) {
        ESP_LOGW(TAG, "overflow: %u + %u > %u", r->received, h.chunk_len, r->total_len);
        blifi_reasm_reset(r);
        return BLIFI_REASM_ERROR;
    }
    memcpy(&r->buf[r->received], chunk, h.chunk_len);
    r->received += h.chunk_len;
    r->next_seq++;

    if (r->received == r->total_len) {
        r->active = false;
        return BLIFI_REASM_COMPLETE;
    }
    return BLIFI_REASM_PARTIAL;
}
