// SPDX-License-Identifier: MIT
#include "ibus.h"

#define IBUS_HEADER_LEN  0x20
#define IBUS_HEADER_TYPE 0x40

uint16_t ibus_checksum(const uint8_t *data, size_t len)
{
    uint16_t sum = 0xFFFF;
    if (!data)
        return sum;
    for (size_t i = 0; i < len; i++)
        sum -= data[i];
    return sum;
}

ibus_err_t ibus_decode(const uint8_t *data, size_t len, ibus_frame_t *out)
{
    if (!data || !out)
        return IBUS_ERR_PARAM;
    if (len < IBUS_FRAME_LEN)
        return IBUS_ERR_SHORT;
    if (data[0] != IBUS_HEADER_LEN || data[1] != IBUS_HEADER_TYPE)
        return IBUS_ERR_HEADER;

    uint16_t expected = ibus_checksum(data, IBUS_FRAME_LEN - 2);
    uint16_t received = (uint16_t)((uint16_t)data[31] << 8 | data[30]);
    if (expected != received)
        return IBUS_ERR_CHECKSUM;

    for (int i = 0; i < IBUS_NUM_CHANNELS; i++) {
        size_t off = 2 + (size_t)i * 2;
        out->channels[i] = (uint16_t)((uint16_t)data[off + 1] << 8 | data[off]);
    }

    return IBUS_OK;
}

ibus_err_t ibus_encode(const ibus_frame_t *frame, uint8_t *buf, size_t cap)
{
    if (!frame || !buf)
        return IBUS_ERR_PARAM;
    if (cap < IBUS_FRAME_LEN)
        return IBUS_ERR_SHORT;

    buf[0] = IBUS_HEADER_LEN;
    buf[1] = IBUS_HEADER_TYPE;

    for (int i = 0; i < IBUS_NUM_CHANNELS; i++) {
        size_t off = 2 + (size_t)i * 2;
        buf[off]     = (uint8_t)(frame->channels[i] & 0xFF);
        buf[off + 1] = (uint8_t)(frame->channels[i] >> 8);
    }

    uint16_t chk = ibus_checksum(buf, IBUS_FRAME_LEN - 2);
    buf[30] = (uint8_t)(chk & 0xFF);
    buf[31] = (uint8_t)(chk >> 8);

    return IBUS_OK;
}
