/**
 * @file frame.h
 * @brief blifi transport framing: 8-byte header, chunking, reassembly.
 *        Implements docs/protocol-spec.md §3 exactly (big-endian header).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLIFI_PROTO_VERSION    0x01
#define BLIFI_FRAME_HEADER_LEN 8
/** Cap on a reassembled inbound payload. All app→dev messages are small. */
#define BLIFI_MAX_MESSAGE_LEN  1024

/** Parsed frame header (§3). */
typedef struct {
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t seq;
    uint16_t total_len;
    uint16_t chunk_len;
} blifi_frame_header_t;

/**
 * @brief Serialize one frame: 8-byte big-endian header followed by the chunk.
 * @return ESP_OK, or ESP_ERR_INVALID_SIZE if `out` is too small.
 */
esp_err_t blifi_frame_pack(const blifi_frame_header_t *hdr, const uint8_t *chunk,
                           uint8_t *out, size_t out_cap, size_t *out_len);

/** Sink for serialized frames (e.g. a BLE notification). */
typedef esp_err_t (*blifi_frame_emit_fn)(void *ctx, const uint8_t *frame, size_t len);

/**
 * @brief Split a payload into frames of at most `max_chunk` bytes and emit each.
 *        A zero-length payload emits a single empty frame (seq=0, total_len=0).
 */
esp_err_t blifi_frame_send(uint8_t msg_type, const uint8_t *payload, size_t len,
                           size_t max_chunk, blifi_frame_emit_fn emit, void *ctx);

/** Per-characteristic reassembly buffer. Zero-initialise before first use. */
typedef struct {
    bool     active;
    uint8_t  msg_type;
    uint16_t total_len;
    uint16_t received;
    uint16_t next_seq;
    uint8_t  buf[BLIFI_MAX_MESSAGE_LEN];
} blifi_reasm_t;

typedef enum {
    BLIFI_REASM_ERROR    = -1, /*!< malformed frame; buffer reset */
    BLIFI_REASM_PARTIAL  = 0,  /*!< accepted, more frames expected */
    BLIFI_REASM_COMPLETE = 1,  /*!< full message in `buf` (`total_len` bytes) */
} blifi_reasm_status_t;

void blifi_reasm_reset(blifi_reasm_t *r);

/**
 * @brief Feed one received frame. On ::BLIFI_REASM_COMPLETE, `r->buf` holds
 *        `r->total_len` bytes of type `r->msg_type`.
 */
blifi_reasm_status_t blifi_reasm_feed(blifi_reasm_t *r, const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif
