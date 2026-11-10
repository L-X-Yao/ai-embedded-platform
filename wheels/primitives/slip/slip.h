// SPDX-License-Identifier: MIT
/*
 * slip — SLIP (Serial Line Internet Protocol) framing encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * RFC 1055 byte stuffing with END/ESC framing. Supports block and
 * streaming modes. Streaming decoder enables byte-at-a-time UART
 * reception (feed from ISR/DMA, check ready in task loop).
 * Thread safety: block functions are pure / reentrant. Streaming
 * decoder requires external locking if fed from multiple contexts.
 */

#ifndef SLIP_H
#define SLIP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SLIP_OK           =  0,
    SLIP_ERR_PARAM   = -1,
    SLIP_ERR_OVERFLOW = -2,
    SLIP_ERR_INVALID  = -3,
    SLIP_ERR_DROPPED  = -4,  /* streaming: this byte is lost, see slip_decoder_feed() */
} slip_err_t;

/* SLIP special bytes (RFC 1055) */
#define SLIP_END      0xC0u
#define SLIP_ESC      0xDBu
#define SLIP_ESC_END  0xDCu  /* ESC + ESC_END = END byte in data */
#define SLIP_ESC_ESC  0xDDu  /* ESC + ESC_ESC = ESC byte in data */

/**
 * Encode data with SLIP framing (adds END delimiters at start and end).
 *
 * @param data      Input buffer. NULL only valid when data_len == 0.
 * @param data_len  Input length in bytes.
 * @param out       Output buffer. Must be at least slip_max_encoded_size(data_len).
 * @param out_len   On input: buffer capacity. On output: bytes written.
 * @return          SLIP_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
slip_err_t slip_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len);

/**
 * Decode SLIP-framed data (removes framing and unescapes).
 * Input must be a complete SLIP frame (with or without leading END).
 *
 * @param encoded   SLIP-encoded input. NULL only valid when enc_len == 0.
 * @param enc_len   Encoded input length in bytes.
 * @param out       Output buffer for decoded data.
 * @param out_len   On input: buffer capacity. On output: bytes written.
 * @return          SLIP_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
slip_err_t slip_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len);

/**
 * Calculate maximum encoded size for a given input length.
 * Worst case: every byte is END or ESC (doubles) + 2 delimiters.
 *
 * @param data_len  Input length in bytes.
 * @return          Maximum encoded output size.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint16_t slip_max_encoded_size(uint16_t data_len);

/* ── streaming decoder ──────────────────────────────────────────────── */

/**
 * Streaming decoder state for byte-at-a-time reception.
 * All fields are internal — do not access directly.
 */
typedef struct {
    uint8_t  *buf;          /* internal: output buffer */
    uint16_t  capacity;     /* internal: buffer capacity */
    uint16_t  pos;          /* internal: current write position */
    bool      escaped;      /* internal: previous byte was ESC */
    bool      frame_ready;  /* internal: complete frame received */
    bool      discarding;   /* internal: dropping bytes up to the next END */
} slip_decoder_t;

/**
 * Initialize streaming decoder with caller-provided buffer.
 *
 * @param dec       Decoder instance.
 * @param buf       Buffer for decoded frame data.
 * @param capacity  Buffer size in bytes.
 * @return          SLIP_OK on success, SLIP_ERR_PARAM if NULL.
 * @thread_safe: no (call once before use) @isr_safe: no
 */
slip_err_t slip_decoder_init(slip_decoder_t *dec, uint8_t *buf, uint16_t capacity);

/**
 * Feed one byte to the streaming decoder.
 * When a complete frame is received, slip_decoder_ready() returns true.
 *
 * SLIP has no checksum, so a frame that lost a byte must not come out at all.
 * After SLIP_ERR_OVERFLOW or SLIP_ERR_INVALID every byte up to the next END is
 * dropped. So is a byte fed while a frame waits for reset, along with the rest
 * of its frame. Call reset promptly: frames arriving before it are lost, and
 * each lost byte returns SLIP_ERR_DROPPED. The END itself returns SLIP_OK, and
 * the frame after it comes through as usual.
 *
 * @param dec   Decoder instance.
 * @param byte  Received byte.
 * @return      SLIP_OK normally, SLIP_ERR_OVERFLOW if frame exceeds buffer,
 *              SLIP_ERR_INVALID if bad escape sequence, SLIP_ERR_DROPPED if
 *              this byte was dropped.
 * @thread_safe: no (use external lock if fed from ISR + task) @isr_safe: yes
 */
slip_err_t slip_decoder_feed(slip_decoder_t *dec, uint8_t byte);

/**
 * Check if a complete frame has been received.
 *
 * @param dec   Decoder instance.
 * @return      true if a frame is ready for retrieval.
 * @thread_safe: no (read-only, but pairs with feed) @isr_safe: yes
 */
bool slip_decoder_ready(const slip_decoder_t *dec);

/**
 * Get pointer and length of the decoded frame.
 * Valid until the next feed() or reset() call.
 *
 * @param dec   Decoder instance.
 * @param data  Set to point at the frame data (internal buffer).
 * @return      Frame length in bytes, 0 if no frame ready.
 * @thread_safe: no @isr_safe: yes
 */
uint16_t slip_decoder_frame(const slip_decoder_t *dec, const uint8_t **data);

/**
 * Reset decoder for the next frame. A frame that lost a byte stays lost: its
 * drop goes on to the next END, as the rest of it is still arriving.
 * slip_decoder_init() starts afresh instead.
 *
 * @param dec   Decoder instance.
 * @thread_safe: no @isr_safe: no
 */
void slip_decoder_reset(slip_decoder_t *dec);

#ifdef __cplusplus
}
#endif

#endif /* SLIP_H */
