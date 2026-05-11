// SPDX-License-Identifier: MIT
/*
 * ppp_frame — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef PPP_FRAME_H
#define PPP_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    PPP_FRAME_OK           =  0,
    PPP_FRAME_ERR_PARAM    = -1,
    PPP_FRAME_ERR_FORMAT   = -2,
    PPP_FRAME_ERR_SHORT    = -3,
    PPP_FRAME_ERR_OVERFLOW = -4,
} ppp_frame_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} ppp_frame_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
ppp_frame_err_t ppp_frame_parse(const uint8_t *data, size_t len,
                               ppp_frame_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int ppp_frame_build(uint8_t *buf, size_t cap,
                        const ppp_frame_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* PPP_FRAME_H */
