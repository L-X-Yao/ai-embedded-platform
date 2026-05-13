// SPDX-License-Identifier: MIT
/*
 * j1939 — SAE J1939 CAN identifier and PGN codec
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Encode/decode J1939 29-bit CAN identifiers: priority, PGN (PDU format,
 * PDU specific, data page), source address, destination address.
 * PDU1 (PF < 240) = peer-to-peer, PDU2 (PF >= 240) = broadcast.
 * Pure C99, no heap, no OS.
 */

#ifndef J1939_H
#define J1939_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define J1939_ADDR_GLOBAL 0xFF

typedef enum {
    J1939_OK           =  0,
    J1939_ERR_PARAM    = -1,
    J1939_ERR_FORMAT   = -2,
} j1939_err_t;

typedef struct {
    uint8_t  priority;
    uint32_t pgn;
    uint8_t  source;
    uint8_t  destination;
    bool     pdu1;
} j1939_id_t;

/** Decode a 29-bit CAN ID into J1939 fields. @thread_safe: yes @isr_safe: yes */
j1939_err_t j1939_decode_id(uint32_t can_id, j1939_id_t *out);

/** Encode J1939 fields into a 29-bit CAN ID. @thread_safe: yes @isr_safe: yes */
j1939_err_t j1939_encode_id(const j1939_id_t *fields, uint32_t *can_id);

/** Extract PGN from a 29-bit CAN ID. @thread_safe: yes @isr_safe: yes */
uint32_t j1939_pgn_from_id(uint32_t can_id);

/** Check if PGN is PDU1 (peer-to-peer, PF < 240). @thread_safe: yes @isr_safe: yes */
bool j1939_is_pdu1(uint32_t pgn);

#ifdef __cplusplus
}
#endif

#endif /* J1939_H */
