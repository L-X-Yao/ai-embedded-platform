// SPDX-License-Identifier: MIT
/*
 * hdlc -- HDLC frame encoding/decoding with byte stuffing and FCS-16
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef HDLC_H
#define HDLC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define HDLC_FLAG       0x7E
#define HDLC_ESCAPE     0x7D
#define HDLC_XOR_MASK   0x20

#define HDLC_FCS_INIT   0xFFFF
#define HDLC_FCS_GOOD   0xF0B8  /* residue after FCS verification */

/* ---------- error codes ---------- */

typedef enum {
    HDLC_OK           =  0,
    HDLC_ERR_PARAM    = -1,
    HDLC_ERR_FORMAT   = -2,
    HDLC_ERR_OVERFLOW = -3,
    HDLC_ERR_FCS      = -4,
} hdlc_err_t;

/* ---------- parser states ---------- */

typedef enum {
    HDLC_STATE_IDLE,
    HDLC_STATE_ACTIVE,
    HDLC_STATE_ESCAPE,
    HDLC_STATE_COMPLETE,
    HDLC_STATE_ERROR,
} hdlc_state_t;

/* ---------- byte-at-a-time parser ---------- */

typedef struct {
    uint8_t    *buf;
    size_t      capacity;
    size_t      idx;
    hdlc_state_t state;
} hdlc_parser_t;

/* ---------- API ---------- */

/** Compute FCS-16 (CRC-16-CCITT reflected) over data.
 *  @thread_safe: yes  @isr_safe: yes */
uint16_t hdlc_fcs16(const uint8_t *data, size_t len);

/** Encode data into an HDLC frame with byte stuffing and FCS-16.
 *  Output: [0x7E] [stuffed(data + FCS16)] [0x7E]
 *  Caller provides address + control + info as data.
 *  @param data      Payload bytes (address + control + info)
 *  @param data_len  Length of payload
 *  @param out       Output buffer
 *  @param out_cap   Capacity of output buffer
 *  @param out_len   Receives actual bytes written
 *  @return HDLC_OK on success
 *  @thread_safe: yes  @isr_safe: no */
hdlc_err_t hdlc_encode(const uint8_t *data, size_t data_len,
                        uint8_t *out, size_t out_cap, size_t *out_len);

/** Initialize parser with a working buffer.
 *  @thread_safe: no  @isr_safe: yes */
hdlc_err_t hdlc_parser_init(hdlc_parser_t *parser, uint8_t *buf, size_t capacity);

/** Feed one byte into the parser. Returns current state.
 *  When HDLC_STATE_COMPLETE, call hdlc_parser_get_frame().
 *  @thread_safe: caller_must_lock  @isr_safe: yes */
hdlc_state_t hdlc_parser_feed(hdlc_parser_t *parser, uint8_t byte);

/** Extract decoded frame from parser (unstuffed, FCS verified + stripped).
 *  Only valid when parser state is HDLC_STATE_COMPLETE.
 *  @param parser    Parser instance
 *  @param data      Output buffer for decoded data
 *  @param data_cap  Capacity of output buffer
 *  @param data_len  Receives actual decoded length
 *  @return HDLC_OK on success
 *  @thread_safe: caller_must_lock  @isr_safe: yes */
hdlc_err_t hdlc_parser_get_frame(const hdlc_parser_t *parser,
                                  uint8_t *data, size_t data_cap,
                                  size_t *data_len);

/** Reset parser to IDLE state.
 *  @thread_safe: no  @isr_safe: yes */
void hdlc_parser_reset(hdlc_parser_t *parser);

#ifdef __cplusplus
}
#endif

#endif /* HDLC_H */
