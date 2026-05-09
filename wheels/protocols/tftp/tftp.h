// SPDX-License-Identifier: MIT
/*
 * tftp — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef TFTP_H
#define TFTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TFTP_OK           =  0,
    TFTP_ERR_PARAM    = -1,
    TFTP_ERR_FORMAT   = -2,
    TFTP_ERR_SHORT    = -3,
    TFTP_ERR_OVERFLOW = -4,
} tftp_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} tftp_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
tftp_err_t tftp_parse(const uint8_t *data, size_t len,
                               tftp_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int tftp_build(uint8_t *buf, size_t cap,
                        const tftp_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* TFTP_H */
