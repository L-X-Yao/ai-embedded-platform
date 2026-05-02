// SPDX-License-Identifier: MIT
/*
 * websocket_frame — WebSocket frame encoder/decoder (RFC 6455)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless frame-level codec. Does NOT handle handshake (use http_parser
 * for that). Supports masking, fragmentation, control frames.
 * Pure C99, no heap.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef WEBSOCKET_FRAME_H
#define WEBSOCKET_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEBSOCKET_FRAME_OK              =  0,
    WEBSOCKET_FRAME_ERR_PARAM       = -1,
    WEBSOCKET_FRAME_ERR_INCOMPLETE  = -2,
    WEBSOCKET_FRAME_ERR_OVERFLOW    = -3,
    WEBSOCKET_FRAME_ERR_PROTOCOL    = -4,
} websocket_frame_err_t;

typedef enum {
    WEBSOCKET_FRAME_OPCODE_CONT   = 0x0,
    WEBSOCKET_FRAME_OPCODE_TEXT   = 0x1,
    WEBSOCKET_FRAME_OPCODE_BINARY = 0x2,
    WEBSOCKET_FRAME_OPCODE_CLOSE  = 0x8,
    WEBSOCKET_FRAME_OPCODE_PING   = 0x9,
    WEBSOCKET_FRAME_OPCODE_PONG   = 0xA,
} websocket_frame_opcode_t;

typedef struct {
    bool        fin;
    websocket_frame_opcode_t opcode;
    bool        masked;
    uint8_t     mask_key[4];
    const uint8_t *payload;
    size_t      payload_len;
    size_t      frame_len;
} websocket_frame_t;

/** @thread_safe: yes (pure function, no state) */
websocket_frame_err_t websocket_frame_parse(const uint8_t *data, size_t len, websocket_frame_t *frame);

/** Build a WebSocket frame into buf; writes total frame length to out_len. */
websocket_frame_err_t websocket_frame_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                        bool fin, websocket_frame_opcode_t opcode,
                        const uint8_t *mask_key,
                        const uint8_t *payload, size_t payload_len);

/** Apply or remove XOR masking on payload data in-place. */
void websocket_frame_mask_payload(uint8_t *data, size_t len, const uint8_t mask_key[4]);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_FRAME_H */
