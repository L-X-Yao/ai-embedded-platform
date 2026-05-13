// SPDX-License-Identifier: MIT
/*
 * isotp — ISO-TP (ISO 15765-2) CAN multi-frame transport protocol
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Encode/decode ISO-TP frames: Single Frame (SF), First Frame (FF),
 * Consecutive Frame (CF), Flow Control (FC). Stateful reassembly for
 * multi-frame messages. Classic CAN (8-byte) payloads.
 * Pure C99, no heap, no OS.
 */

#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ISOTP_CAN_DLC      8
#define ISOTP_MAX_PAYLOAD   4095

typedef enum {
    ISOTP_OK             =  0,
    ISOTP_ERR_PARAM      = -1,
    ISOTP_ERR_FORMAT     = -2,
    ISOTP_ERR_SHORT      = -3,
    ISOTP_ERR_OVERFLOW   = -4,
    ISOTP_ERR_SEQUENCE   = -5,
    ISOTP_INCOMPLETE     =  1,
} isotp_err_t;

typedef enum {
    ISOTP_SF = 0,
    ISOTP_FF = 1,
    ISOTP_CF = 2,
    ISOTP_FC = 3,
} isotp_frame_type_t;

typedef enum {
    ISOTP_FC_CTS      = 0,
    ISOTP_FC_WAIT     = 1,
    ISOTP_FC_OVERFLOW  = 2,
} isotp_fc_flag_t;

typedef struct {
    uint8_t *buf;       /* internal: reassembly buffer */
    size_t   cap;       /* internal: buffer capacity */
    size_t   total_len; /* internal: expected total length */
    size_t   offset;    /* internal: bytes received so far */
    uint8_t  next_sn;   /* internal: expected sequence number */
    bool     active;    /* internal: reassembly in progress */
} isotp_rx_t;

/** Init receiver with a reassembly buffer. @thread_safe: no @isr_safe: no */
isotp_err_t isotp_rx_init(isotp_rx_t *rx, uint8_t *buf, size_t cap);

/**
 * Feed a CAN frame (8 bytes) to the receiver.
 * Returns ISOTP_OK when a complete message is ready (use isotp_rx_get_*).
 * Returns ISOTP_INCOMPLETE when more frames are needed.
 * @thread_safe: no @isr_safe: no
 */
isotp_err_t isotp_rx_feed(isotp_rx_t *rx, const uint8_t *can_data, uint8_t dlc);

/** Get reassembled message length. @thread_safe: no @isr_safe: yes */
size_t isotp_rx_length(const isotp_rx_t *rx);

/** Get pointer to reassembled data. @thread_safe: no @isr_safe: yes */
const uint8_t *isotp_rx_data(const isotp_rx_t *rx);

/** Reset receiver. @thread_safe: no @isr_safe: no */
isotp_err_t isotp_rx_reset(isotp_rx_t *rx);

/** Get frame type from first byte of CAN data. @thread_safe: yes @isr_safe: yes */
isotp_frame_type_t isotp_frame_type(uint8_t pci);

/**
 * Encode a single-frame message (<=7 bytes).
 * Writes 8 bytes to can_data, padding with 0xCC.
 * @thread_safe: yes @isr_safe: yes
 */
isotp_err_t isotp_encode_sf(const uint8_t *payload, size_t len,
                             uint8_t *can_data);

/**
 * Build a Flow Control frame.
 * @thread_safe: yes @isr_safe: yes
 */
isotp_err_t isotp_encode_fc(isotp_fc_flag_t flag, uint8_t block_size,
                             uint8_t st_min, uint8_t *can_data);

#ifdef __cplusplus
}
#endif

#endif /* ISOTP_H */
