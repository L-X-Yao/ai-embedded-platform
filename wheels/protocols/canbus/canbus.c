/*
 * canbus — CAN 2.0A/B frame encoder/decoder with signal extraction
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "canbus.h"
#include <string.h>

bool canbus_is_standard_id(uint32_t id)
{
    return id <= CANBUS_STD_ID_MAX;
}

bool canbus_is_extended_id(uint32_t id)
{
    return id <= CANBUS_EXT_ID_MAX;
}

canbus_err_t canbus_frame_validate(const canbus_frame_t *frame)
{
    if (!frame) return CANBUS_ERR_PARAMS;

    if (frame->dlc > CANBUS_DLC_MAX) return CANBUS_ERR_DLC;

    if (frame->extended) {
        if (frame->id > CANBUS_EXT_ID_MAX) return CANBUS_ERR_ID;
    } else {
        if (frame->id > CANBUS_STD_ID_MAX) return CANBUS_ERR_ID;
    }

    return CANBUS_OK;
}

int canbus_frame_pack(const canbus_frame_t *frame,
                      uint8_t *buf, uint16_t buf_size)
{
    if (!frame || !buf) return CANBUS_ERR_PARAMS;

    canbus_err_t err = canbus_frame_validate(frame);
    if (err != CANBUS_OK) return (int)err;

    uint16_t overhead = frame->extended
                        ? CANBUS_PACK_EXT_OVERHEAD
                        : CANBUS_PACK_STD_OVERHEAD;
    uint8_t data_len = frame->rtr ? 0 : frame->dlc;
    uint16_t total = (uint16_t)(overhead + data_len);

    if (total > buf_size) return CANBUS_ERR_OVERFLOW;

    int pos = 0;

    /* FLAGS_DLC byte — always first for unambiguous type detection */
    uint8_t flags_dlc = frame->dlc & 0x0Fu;
    if (frame->extended) flags_dlc |= CANBUS_FLAG_EXTENDED;
    if (frame->rtr)      flags_dlc |= CANBUS_FLAG_RTR;
    buf[pos++] = flags_dlc;

    /* ID bytes — big-endian */
    if (frame->extended) {
        buf[pos++] = (uint8_t)(frame->id >> 24);
        buf[pos++] = (uint8_t)(frame->id >> 16);
        buf[pos++] = (uint8_t)(frame->id >> 8);
        buf[pos++] = (uint8_t)(frame->id);
    } else {
        buf[pos++] = (uint8_t)(frame->id >> 8);
        buf[pos++] = (uint8_t)(frame->id);
    }

    /* Data — RTR frames carry no data */
    if (!frame->rtr && frame->dlc > 0) {
        memcpy(buf + pos, frame->data, frame->dlc);
        pos += frame->dlc;
    }

    return pos;
}

canbus_err_t canbus_frame_unpack(const uint8_t *buf, uint16_t len,
                                 canbus_frame_t *frame)
{
    if (!buf || !frame) return CANBUS_ERR_PARAMS;

    /* Need at least FLAGS_DLC byte */
    if (len < 1) return CANBUS_ERR_SIZE;

    /* FLAGS_DLC is always byte[0] — unambiguous type detection */
    uint8_t flags_dlc = buf[0];
    frame->extended = (flags_dlc & CANBUS_FLAG_EXTENDED) != 0;
    frame->rtr      = (flags_dlc & CANBUS_FLAG_RTR) != 0;
    frame->dlc      = flags_dlc & 0x0Fu;

    if (frame->dlc > CANBUS_DLC_MAX) return CANBUS_ERR_DLC;

    uint16_t overhead = frame->extended
                        ? CANBUS_PACK_EXT_OVERHEAD
                        : CANBUS_PACK_STD_OVERHEAD;

    if (len < overhead) return CANBUS_ERR_SIZE;

    /* Parse ID — big-endian, starting at byte[1] */
    if (frame->extended) {
        frame->id = ((uint32_t)buf[1] << 24) |
                    ((uint32_t)buf[2] << 16) |
                    ((uint32_t)buf[3] << 8)  |
                    ((uint32_t)buf[4]);
        if (frame->id > CANBUS_EXT_ID_MAX) return CANBUS_ERR_ID;
    } else {
        frame->id = ((uint32_t)buf[1] << 8) | (uint32_t)buf[2];
        if (frame->id > CANBUS_STD_ID_MAX) return CANBUS_ERR_ID;
    }

    /* Data */
    memset(frame->data, 0, sizeof(frame->data));
    if (!frame->rtr && frame->dlc > 0) {
        uint16_t needed = (uint16_t)(overhead + frame->dlc);
        if (len < needed) return CANBUS_ERR_SIZE;
        memcpy(frame->data, buf + overhead, frame->dlc);
    }

    return CANBUS_OK;
}

bool canbus_filter_match(const canbus_filter_t *filter,
                         const canbus_frame_t *frame)
{
    if (!filter || !frame) return false;

    /* Extended flag must match */
    if (filter->extended != frame->extended) return false;

    /* Apply mask: bits where mask=1 must match */
    uint32_t id_max = filter->extended ? CANBUS_EXT_ID_MAX : CANBUS_STD_ID_MAX;
    uint32_t mask = filter->mask & id_max;
    return (filter->id & mask) == (frame->id & mask);
}

uint64_t canbus_signal_extract(const uint8_t *data, uint8_t start_bit,
                               uint8_t length, bool big_endian)
{
    if (!data || length == 0 || length > 64) return 0;
    if ((uint16_t)start_bit + length > 64) return 0;

    uint64_t result = 0;

    if (!big_endian) {
        /* Intel (little-endian) byte order:
         * start_bit is LSB position. Bits are numbered sequentially
         * across bytes: byte0[0-7], byte1[8-15], etc. */
        for (uint8_t i = 0; i < length; i++) {
            uint8_t bit_pos = start_bit + i;
            uint8_t byte_idx = bit_pos / 8u;
            uint8_t bit_in_byte = bit_pos % 8u;
            if (byte_idx >= 8u) break;
            if (data[byte_idx] & (1u << bit_in_byte)) {
                result |= (uint64_t)1 << i;
            }
        }
    } else {
        /* Motorola (big-endian) byte order:
         * start_bit is MSB position. Numbering follows Motorola convention:
         * bit N is in byte (N/8), bit-within-byte (N%8).
         * Consecutive signal bits go: same byte downward, then next byte
         * from bit 7. */
        uint8_t bit_pos = start_bit;
        for (uint8_t i = 0; i < length; i++) {
            uint8_t byte_idx = bit_pos / 8u;
            uint8_t bit_in_byte = bit_pos % 8u;
            if (byte_idx >= 8u) break;
            if (data[byte_idx] & (1u << bit_in_byte)) {
                result |= (uint64_t)1 << (length - 1u - i);
            }
            /* Navigate to next bit in Motorola order */
            if (bit_in_byte == 0) {
                bit_pos = (uint8_t)(bit_pos + 15u);  /* jump to bit 7 of next byte */
            } else {
                bit_pos = (uint8_t)(bit_pos - 1u);
            }
        }
    }

    return result;
}

void canbus_signal_pack(uint8_t *data, uint8_t start_bit,
                        uint8_t length, bool big_endian, uint64_t value)
{
    if (!data || length == 0 || length > 64) return;
    if ((uint16_t)start_bit + length > 64) return;

    if (!big_endian) {
        /* Intel (little-endian) */
        for (uint8_t i = 0; i < length; i++) {
            uint8_t bit_pos = start_bit + i;
            uint8_t byte_idx = bit_pos / 8u;
            uint8_t bit_in_byte = bit_pos % 8u;
            if (byte_idx >= 8u) break;
            if (value & ((uint64_t)1 << i)) {
                data[byte_idx] |= (uint8_t)(1u << bit_in_byte);
            } else {
                data[byte_idx] &= (uint8_t)~(1u << bit_in_byte);
            }
        }
    } else {
        /* Motorola (big-endian) */
        uint8_t bit_pos = start_bit;
        for (uint8_t i = 0; i < length; i++) {
            uint8_t byte_idx = bit_pos / 8u;
            uint8_t bit_in_byte = bit_pos % 8u;
            if (byte_idx >= 8u) break;
            if (value & ((uint64_t)1 << (length - 1u - i))) {
                data[byte_idx] |= (uint8_t)(1u << bit_in_byte);
            } else {
                data[byte_idx] &= (uint8_t)~(1u << bit_in_byte);
            }
            if (bit_in_byte == 0) {
                bit_pos = (uint8_t)(bit_pos + 15u);
            } else {
                bit_pos = (uint8_t)(bit_pos - 1u);
            }
        }
    }
}
