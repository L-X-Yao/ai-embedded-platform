/*
 * websocket_frame — WebSocket frame encoder/decoder (RFC 6455)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
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
    WS_OK              =  0,
    WS_ERR_PARAM       = -1,
    WS_ERR_INCOMPLETE  = -2,
    WS_ERR_OVERFLOW    = -3,
    WS_ERR_PROTOCOL    = -4,
} ws_err_t;

typedef enum {
    WS_OPCODE_CONT   = 0x0,
    WS_OPCODE_TEXT   = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE  = 0x8,
    WS_OPCODE_PING   = 0x9,
    WS_OPCODE_PONG   = 0xA,
} ws_opcode_t;

typedef struct {
    bool        fin;
    ws_opcode_t opcode;
    bool        masked;
    uint8_t     mask_key[4];
    const uint8_t *payload;
    size_t      payload_len;
    size_t      frame_len;
} ws_frame_t;

/** @thread_safe: yes (pure function, no state) */
ws_err_t ws_frame_parse(const uint8_t *data, size_t len, ws_frame_t *frame);

ws_err_t ws_frame_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                        bool fin, ws_opcode_t opcode,
                        const uint8_t *mask_key,
                        const uint8_t *payload, size_t payload_len);

void ws_mask_payload(uint8_t *data, size_t len, const uint8_t mask_key[4]);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_FRAME_H */
