// SPDX-License-Identifier: MIT
/*
 * isotp — ISO-TP (ISO 15765-2) CAN multi-frame transport protocol
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "isotp.h"
#include <string.h>

isotp_frame_type_t isotp_frame_type(uint8_t pci)
{
    return (isotp_frame_type_t)((pci >> 4) & 0x0F);
}

isotp_err_t isotp_rx_init(isotp_rx_t *rx, uint8_t *buf, size_t cap)
{
    if (!rx || !buf)
        return ISOTP_ERR_PARAM;
    rx->buf = buf;
    rx->cap = cap;
    rx->total_len = 0;
    rx->offset = 0;
    rx->next_sn = 0;
    rx->active = false;
    return ISOTP_OK;
}

isotp_err_t isotp_rx_feed(isotp_rx_t *rx, const uint8_t *can_data, uint8_t dlc)
{
    if (!rx || !can_data)
        return ISOTP_ERR_PARAM;
    if (dlc < 1 || dlc > ISOTP_CAN_DLC)
        return ISOTP_ERR_SHORT;

    isotp_frame_type_t ft = isotp_frame_type(can_data[0]);

    switch (ft) {
    case ISOTP_SF: {
        uint8_t sf_len = can_data[0] & 0x0F;
        if (sf_len == 0 || sf_len > 7)
            return ISOTP_ERR_FORMAT;
        if ((size_t)sf_len > (size_t)(dlc - 1))
            return ISOTP_ERR_SHORT;
        if ((size_t)sf_len > rx->cap)
            return ISOTP_ERR_OVERFLOW;
        memcpy(rx->buf, &can_data[1], sf_len);
        rx->total_len = sf_len;
        rx->offset = sf_len;
        rx->active = false;
        return ISOTP_OK;
    }

    case ISOTP_FF: {
        if (dlc < 2)
            return ISOTP_ERR_SHORT;
        uint16_t ff_len = (uint16_t)(((uint16_t)(can_data[0] & 0x0F) << 8) | can_data[1]);
        if (ff_len < 8)
            return ISOTP_ERR_FORMAT;
        if ((size_t)ff_len > rx->cap)
            return ISOTP_ERR_OVERFLOW;
        size_t first_bytes = (dlc > 2) ? (size_t)(dlc - 2) : 0;
        if (first_bytes > (size_t)ff_len)
            first_bytes = (size_t)ff_len;
        memcpy(rx->buf, &can_data[2], first_bytes);
        rx->total_len = ff_len;
        rx->offset = first_bytes;
        rx->next_sn = 1;
        rx->active = true;
        return ISOTP_INCOMPLETE;
    }

    case ISOTP_CF: {
        if (!rx->active)
            return ISOTP_ERR_SEQUENCE;
        uint8_t sn = can_data[0] & 0x0F;
        if (sn != rx->next_sn) {
            /* ISO-TP has no CRC, so a later frame that happened to match
             * would join two messages. Abort, as Linux isotp_rcv_cf does. */
            rx->active = false;
            return ISOTP_ERR_SEQUENCE;
        }
        size_t remaining = rx->total_len - rx->offset;
        size_t copy = (size_t)(dlc - 1);
        if (copy > remaining)
            copy = remaining;
        memcpy(&rx->buf[rx->offset], &can_data[1], copy);
        rx->offset += copy;
        rx->next_sn = (uint8_t)((rx->next_sn + 1) & 0x0F);
        if (rx->offset >= rx->total_len) {
            rx->active = false;
            return ISOTP_OK;
        }
        return ISOTP_INCOMPLETE;
    }

    case ISOTP_FC:
        /* Flow control carries no message data, so nothing became ready. */
        return ISOTP_INCOMPLETE;

    default:
        return ISOTP_ERR_FORMAT;
    }
}

size_t isotp_rx_length(const isotp_rx_t *rx)
{
    if (!rx)
        return 0;
    return rx->total_len;
}

const uint8_t *isotp_rx_data(const isotp_rx_t *rx)
{
    if (!rx)
        return NULL;
    return rx->buf;
}

isotp_err_t isotp_rx_reset(isotp_rx_t *rx)
{
    if (!rx)
        return ISOTP_ERR_PARAM;
    rx->total_len = 0;
    rx->offset = 0;
    rx->next_sn = 0;
    rx->active = false;
    return ISOTP_OK;
}

isotp_err_t isotp_encode_sf(const uint8_t *payload, size_t len,
                             uint8_t *can_data)
{
    if (!payload || !can_data)
        return ISOTP_ERR_PARAM;
    if (len == 0 || len > 7)
        return ISOTP_ERR_OVERFLOW;
    memset(can_data, 0xCC, ISOTP_CAN_DLC);
    can_data[0] = (uint8_t)len;
    memcpy(&can_data[1], payload, len);
    return ISOTP_OK;
}

isotp_err_t isotp_encode_fc(isotp_fc_flag_t flag, uint8_t block_size,
                             uint8_t st_min, uint8_t *can_data)
{
    if (!can_data)
        return ISOTP_ERR_PARAM;
    if ((unsigned)flag > 2)
        return ISOTP_ERR_PARAM;
    memset(can_data, 0xCC, ISOTP_CAN_DLC);
    can_data[0] = (uint8_t)(0x30 | (uint8_t)flag);
    can_data[1] = block_size;
    can_data[2] = st_min;
    return ISOTP_OK;
}
