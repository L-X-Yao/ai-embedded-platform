// SPDX-License-Identifier: MIT
/*
 * ubx_parser — TODO: one-line description
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

/* ---------- error codes ---------- */

typedef enum {
    UBX_PARSER_OK           =  0,
    UBX_PARSER_ERR_PARAM    = -1,
    UBX_PARSER_ERR_FORMAT   = -2,
    UBX_PARSER_ERR_SHORT    = -3,
    UBX_PARSER_ERR_OVERFLOW = -4,
} ubx_parser_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} ubx_parser_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
ubx_parser_err_t ubx_parser_parse(const uint8_t *data, size_t len,
                               ubx_parser_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int ubx_parser_build(uint8_t *buf, size_t cap,
                        const ubx_parser_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* UBX_PARSER_H */
