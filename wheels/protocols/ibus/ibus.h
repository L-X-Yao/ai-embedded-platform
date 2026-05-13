// SPDX-License-Identifier: MIT
/*
 * ibus — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef IBUS_H
#define IBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    IBUS_OK           =  0,
    IBUS_ERR_PARAM    = -1,
    IBUS_ERR_FORMAT   = -2,
    IBUS_ERR_SHORT    = -3,
    IBUS_ERR_OVERFLOW = -4,
} ibus_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} ibus_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
ibus_err_t ibus_parse(const uint8_t *data, size_t len,
                               ibus_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int ibus_build(uint8_t *buf, size_t cap,
                        const ibus_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_H */
