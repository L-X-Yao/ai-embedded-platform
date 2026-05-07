// SPDX-License-Identifier: MIT
/*
 * sbus -- Futaba S.Bus RC receiver protocol parser and encoder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef SBUS_H
#define SBUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define SBUS_NUM_CHANNELS    16
#define SBUS_FRAME_LEN       25
#define SBUS_HEADER          0x0F
#define SBUS_FOOTER          0x00

/* Channel value range (11-bit) */
#define SBUS_CHANNEL_MIN     0
#define SBUS_CHANNEL_MAX     2047

/* Typical RC range mapping */
#define SBUS_RC_MIN          172
#define SBUS_RC_MAX          1811
#define SBUS_US_MIN          1000
#define SBUS_US_MAX          2000

/* Flag bits in byte 23 */
#define SBUS_FLAG_CH17       0x01
#define SBUS_FLAG_CH18       0x02
#define SBUS_FLAG_FRAME_LOST 0x04
#define SBUS_FLAG_FAILSAFE   0x08

/* ---------- error codes ---------- */

typedef enum {
    SBUS_OK           =  0,
    SBUS_ERR_PARAM    = -1,
    SBUS_ERR_FORMAT   = -2,
} sbus_err_t;

/* ---------- parsed frame ---------- */

typedef struct {
    uint16_t channels[SBUS_NUM_CHANNELS];  /* 0-2047 */
    bool     ch17;
    bool     ch18;
    bool     frame_lost;
    bool     failsafe;
} sbus_frame_t;

/* ---------- byte-at-a-time parser ---------- */

typedef enum {
    SBUS_PARSE_SEEKING,
    SBUS_PARSE_IN_FRAME,
    SBUS_PARSE_COMPLETE,
    SBUS_PARSE_ERROR,
} sbus_parse_state_t;

typedef struct {
    uint8_t buf[SBUS_FRAME_LEN];
    uint8_t idx;
    sbus_parse_state_t state;
} sbus_parser_t;

/* ---------- API ---------- */

/** Initialize parser to SEEKING state. @thread_safe: no @isr_safe: yes */
sbus_err_t sbus_parser_init(sbus_parser_t *parser);

/** Feed one byte into the parser. Returns current state.
 *  When SBUS_PARSE_COMPLETE is returned, call sbus_get_frame().
 *  @thread_safe: caller_must_lock @isr_safe: yes */
sbus_parse_state_t sbus_parse_byte(sbus_parser_t *parser, uint8_t byte);

/** Decode a complete 25-byte frame into structured fields.
 *  @thread_safe: yes @isr_safe: yes */
sbus_err_t sbus_decode(const uint8_t frame[SBUS_FRAME_LEN], sbus_frame_t *out);

/** Encode structured fields into a 25-byte frame.
 *  @thread_safe: yes @isr_safe: yes */
sbus_err_t sbus_encode(const sbus_frame_t *frame, uint8_t buf[SBUS_FRAME_LEN]);

/** Get pointer to decoded frame from parser buffer. Only valid when
 *  parser state is SBUS_PARSE_COMPLETE. Caller must decode with sbus_decode().
 *  Returns pointer to internal buffer (25 bytes).
 *  @thread_safe: caller_must_lock @isr_safe: yes */
const uint8_t *sbus_parser_get_buf(const sbus_parser_t *parser);

/* ---------- utility ---------- */

/** Map S-Bus channel value (172-1811) to microseconds (1000-2000).
 *  Values outside range are clamped. @thread_safe: yes @isr_safe: yes */
uint16_t sbus_channel_to_us(uint16_t channel_value);

/** Map microseconds (1000-2000) to S-Bus channel value (172-1811).
 *  Values outside range are clamped. @thread_safe: yes @isr_safe: yes */
uint16_t sbus_us_to_channel(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif /* SBUS_H */
