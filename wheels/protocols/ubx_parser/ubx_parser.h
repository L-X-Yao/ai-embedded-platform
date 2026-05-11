// SPDX-License-Identifier: MIT
/*
 * ubx_parser -- u-blox UBX binary GNSS protocol parser/builder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef UBX_PARSER_H
#define UBX_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define UBX_SYNC1  0xB5
#define UBX_SYNC2  0x62

#define UBX_HEADER_LEN   6   /* sync1 + sync2 + class + id + length(2) */
#define UBX_OVERHEAD      8   /* header(6) + ckA(1) + ckB(1) */

/* Common UBX message classes */
#define UBX_CLASS_NAV   0x01
#define UBX_CLASS_ACK   0x05
#define UBX_CLASS_CFG   0x06

/* Common NAV message IDs */
#define UBX_NAV_PVT     0x07

/* ACK message IDs */
#define UBX_ACK_ACK     0x01
#define UBX_ACK_NAK     0x00

#ifndef UBX_PARSER_MAX_PAYLOAD
#define UBX_PARSER_MAX_PAYLOAD  256
#endif

/* ---------- error codes ---------- */

typedef enum {
    UBX_PARSER_OK           =  0,
    UBX_PARSER_ERR_PARAM    = -1,
    UBX_PARSER_ERR_CHECKSUM = -2,
    UBX_PARSER_ERR_SHORT    = -3,
    UBX_PARSER_ERR_OVERFLOW = -4,
} ubx_parser_err_t;

/* ---------- feed status ---------- */

typedef enum {
    UBX_FEED_IDLE = 0,
    UBX_FEED_SYNCING,
    UBX_FEED_HEADER,
    UBX_FEED_PAYLOAD,
    UBX_FEED_COMPLETE,
    UBX_FEED_ERROR,
} ubx_feed_status_t;

/* ---------- data types ---------- */

/** Parsed UBX message (zero-copy: payload points into context buffer). */
typedef struct {
    uint8_t        msg_class;
    uint8_t        msg_id;
    const uint8_t *payload;
    uint16_t       payload_len;
} ubx_msg_t;

/** Incremental parser context. */
typedef struct {
    uint8_t       *buf;
    size_t         buf_size;
    size_t         idx;
    uint16_t       payload_len;
    ubx_feed_status_t state;
} ubx_parser_ctx_t;

/* ---------- API ---------- */

/** Initialize parser context with caller-provided buffer.
 *  @thread_safe: no  @isr_safe: no */
ubx_parser_err_t ubx_parser_init(ubx_parser_ctx_t *ctx,
                                  uint8_t *buf, size_t buf_size);

/** Feed one byte into the incremental parser.
 *  Returns current state; UBX_FEED_COMPLETE when a full message is ready.
 *  @thread_safe: no  @isr_safe: yes (no blocking) */
ubx_feed_status_t ubx_parser_feed(ubx_parser_ctx_t *ctx, uint8_t byte);

/** Extract the parsed message after feed() returns COMPLETE.
 *  @thread_safe: no  @isr_safe: no */
ubx_parser_err_t ubx_parser_get_msg(const ubx_parser_ctx_t *ctx,
                                     ubx_msg_t *out);

/** Reset parser to IDLE state for next message.
 *  @thread_safe: no  @isr_safe: no */
void ubx_parser_reset(ubx_parser_ctx_t *ctx);

/** Build a complete UBX frame into buf.
 *  Returns total bytes written, or negative error.
 *  @thread_safe: yes  @isr_safe: no */
int ubx_parser_build(uint8_t *buf, size_t cap,
                     uint8_t msg_class, uint8_t msg_id,
                     const uint8_t *payload, uint16_t payload_len);

/** Compute UBX Fletcher-8 checksum over data[0..len-1].
 *  @thread_safe: yes  @isr_safe: yes */
void ubx_parser_checksum(const uint8_t *data, size_t len,
                         uint8_t *ck_a, uint8_t *ck_b);

#ifdef __cplusplus
}
#endif

#endif /* UBX_PARSER_H */
