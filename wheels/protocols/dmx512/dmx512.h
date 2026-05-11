// SPDX-License-Identifier: MIT
/*
 * dmx512 — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef DMX512_H
#define DMX512_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DMX512_OK           =  0,
    DMX512_ERR_PARAM    = -1,
    DMX512_ERR_FORMAT   = -2,
    DMX512_ERR_SHORT    = -3,
    DMX512_ERR_OVERFLOW = -4,
} dmx512_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} dmx512_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
dmx512_err_t dmx512_parse(const uint8_t *data, size_t len,
                               dmx512_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int dmx512_build(uint8_t *buf, size_t cap,
                        const dmx512_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* DMX512_H */
