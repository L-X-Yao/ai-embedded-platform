// SPDX-License-Identifier: MIT
/*
 * crsf — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CRSF_OK           =  0,
    CRSF_ERR_PARAM    = -1,
    CRSF_ERR_FORMAT   = -2,
    CRSF_ERR_SHORT    = -3,
    CRSF_ERR_OVERFLOW = -4,
} crsf_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} crsf_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
crsf_err_t crsf_parse(const uint8_t *data, size_t len,
                               crsf_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int crsf_build(uint8_t *buf, size_t cap,
                        const crsf_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* CRSF_H */
