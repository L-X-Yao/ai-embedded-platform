// SPDX-License-Identifier: MIT
/*
 * dhcp_packet — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef DHCP_PACKET_H
#define DHCP_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DHCP_PACKET_OK           =  0,
    DHCP_PACKET_ERR_PARAM    = -1,
    DHCP_PACKET_ERR_FORMAT   = -2,
    DHCP_PACKET_ERR_SHORT    = -3,
    DHCP_PACKET_ERR_OVERFLOW = -4,
} dhcp_packet_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} dhcp_packet_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
dhcp_packet_err_t dhcp_packet_parse(const uint8_t *data, size_t len,
                               dhcp_packet_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int dhcp_packet_build(uint8_t *buf, size_t cap,
                        const dhcp_packet_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* DHCP_PACKET_H */
