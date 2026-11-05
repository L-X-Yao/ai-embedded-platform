// SPDX-License-Identifier: MIT
/*
 * crsf — TBS Crossfire / ExpressLRS (CRSF) RC protocol
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "crsf.h"
#include <string.h>

uint8_t crsf_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    if (!data)
        return crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0xD5);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

crsf_err_t crsf_decode_frame(const uint8_t *data, size_t len,
                              crsf_frame_t *out)
{
    if (!data || !out)
        return CRSF_ERR_PARAM;
    if (len < 4)
        return CRSF_ERR_SHORT;

    uint8_t addr = data[0];
    uint8_t frame_len = data[1];

    if ((size_t)frame_len + 2 > len)
        return CRSF_ERR_SHORT;
    if (frame_len < 2)
        return CRSF_ERR_SHORT;

    uint8_t type = data[2];
    uint8_t payload_len = (uint8_t)(frame_len - 2);

    if (payload_len > CRSF_MAX_FRAME_SIZE)
        return CRSF_ERR_OVERFLOW;

    /* CRC over type + payload */
    uint8_t expected_crc = crsf_crc8(&data[2], (size_t)frame_len - 1);
    uint8_t received_crc = data[2 + frame_len - 1];
    if (expected_crc != received_crc)
        return CRSF_ERR_CRC;

    out->addr = addr;
    out->type = type;
    out->payload_len = payload_len;
    if (payload_len > 0)
        memcpy(out->payload, &data[3], payload_len);

    return CRSF_OK;
}

crsf_err_t crsf_encode_frame(const crsf_frame_t *frame,
                              uint8_t *buf, size_t cap, size_t *out_len)
{
    if (!frame || !buf || !out_len)
        return CRSF_ERR_PARAM;
    if (frame->payload_len > CRSF_MAX_FRAME_SIZE)
        return CRSF_ERR_OVERFLOW;

    size_t total = (size_t)frame->payload_len + 4;
    if (total > cap)
        return CRSF_ERR_OVERFLOW;

    buf[0] = frame->addr;
    buf[1] = (uint8_t)(frame->payload_len + 2);
    buf[2] = frame->type;
    if (frame->payload_len > 0)
        memcpy(&buf[3], frame->payload, frame->payload_len);

    /* CRC over type + payload */
    buf[3 + frame->payload_len] = crsf_crc8(&buf[2],
                                             (size_t)frame->payload_len + 1);
    *out_len = total;
    return CRSF_OK;
}

crsf_err_t crsf_unpack_channels(const uint8_t *payload, size_t len,
                                 crsf_channels_t *out)
{
    if (!payload || !out)
        return CRSF_ERR_PARAM;
    if (len < CRSF_CHANNEL_BYTES)
        return CRSF_ERR_SHORT;

    /* 16 channels × 11 bits = 176 bits = 22 bytes, LSB first */
    uint32_t bits = 0;
    int nbits = 0;
    int byte_idx = 0;
    for (int ch = 0; ch < CRSF_NUM_CHANNELS; ch++) {
        while (nbits < 11) {
            bits |= (uint32_t)payload[byte_idx++] << nbits;
            nbits += 8;
        }
        out->channels[ch] = (uint16_t)(bits & 0x7FF);
        bits >>= 11;
        nbits -= 11;
    }

    return CRSF_OK;
}

crsf_err_t crsf_pack_channels(const crsf_channels_t *channels,
                               uint8_t *payload, size_t cap)
{
    if (!channels || !payload)
        return CRSF_ERR_PARAM;
    if (cap < CRSF_CHANNEL_BYTES)
        return CRSF_ERR_OVERFLOW;

    memset(payload, 0, CRSF_CHANNEL_BYTES);

    uint32_t bits = 0;
    int nbits = 0;
    int byte_idx = 0;
    for (int ch = 0; ch < CRSF_NUM_CHANNELS; ch++) {
        bits |= ((uint32_t)channels->channels[ch] & 0x7FF) << nbits;
        nbits += 11;
        while (nbits >= 8) {
            payload[byte_idx++] = (uint8_t)(bits & 0xFF);
            bits >>= 8;
            nbits -= 8;
        }
    }

    return CRSF_OK;
}
