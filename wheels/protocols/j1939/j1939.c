// SPDX-License-Identifier: MIT
/*
 * j1939 — SAE J1939 CAN identifier and PGN codec
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "j1939.h"

/*
 * 29-bit CAN ID layout (MSB to LSB):
 *   [28:26] Priority (3 bits)
 *   [25]    Reserved (always 0 in J1939)
 *   [24]    Data Page
 *   [23:16] PDU Format (PF)
 *   [15:8]  PDU Specific (PS) — destination addr (PDU1) or group ext (PDU2)
 *   [7:0]   Source Address
 *
 * PGN (18 bits): DataPage[17] | PF[16:9] | PS[8:1] for PDU2
 *                DataPage[17] | PF[16:9] | 0x00    for PDU1 (PF < 240)
 */

bool j1939_is_pdu1(uint32_t pgn)
{
    uint8_t pf = (uint8_t)((pgn >> 8) & 0xFF);
    return pf < 240;
}

uint32_t j1939_pgn_from_id(uint32_t can_id)
{
    uint8_t dp = (uint8_t)((can_id >> 24) & 0x01);
    uint8_t pf = (uint8_t)((can_id >> 16) & 0xFF);
    uint8_t ps = (uint8_t)((can_id >> 8) & 0xFF);

    uint32_t pgn = ((uint32_t)dp << 16) | ((uint32_t)pf << 8);
    if (pf >= 240)
        pgn |= ps;
    return pgn;
}

j1939_err_t j1939_decode_id(uint32_t can_id, j1939_id_t *out)
{
    if (!out)
        return J1939_ERR_PARAM;
    if (can_id > 0x1FFFFFFFu)
        return J1939_ERR_FORMAT;

    out->priority = (uint8_t)((can_id >> 26) & 0x07);
    out->source = (uint8_t)(can_id & 0xFF);

    uint8_t pf = (uint8_t)((can_id >> 16) & 0xFF);
    uint8_t ps = (uint8_t)((can_id >> 8) & 0xFF);

    out->pgn = j1939_pgn_from_id(can_id);

    if (pf < 240) {
        out->pdu1 = true;
        out->destination = ps;
    } else {
        out->pdu1 = false;
        out->destination = J1939_ADDR_GLOBAL;
    }

    return J1939_OK;
}

j1939_err_t j1939_encode_id(const j1939_id_t *fields, uint32_t *can_id)
{
    if (!fields || !can_id)
        return J1939_ERR_PARAM;
    if (fields->priority > 7)
        return J1939_ERR_PARAM;

    uint8_t dp = (uint8_t)((fields->pgn >> 16) & 0x01);
    uint8_t pf = (uint8_t)((fields->pgn >> 8) & 0xFF);
    uint8_t ps;

    if (pf < 240)
        ps = fields->destination;
    else
        ps = (uint8_t)(fields->pgn & 0xFF);

    *can_id = ((uint32_t)fields->priority << 26) |
              ((uint32_t)dp << 24) |
              ((uint32_t)pf << 16) |
              ((uint32_t)ps << 8) |
              fields->source;

    return J1939_OK;
}
