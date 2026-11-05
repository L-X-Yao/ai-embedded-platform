// SPDX-License-Identifier: MIT
/*
 * websocket_frame — WebSocket frame encoder/decoder (RFC 6455)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "websocket_frame.h"
#include "contract.h"
#include <string.h>

void websocket_frame_mask_payload(uint8_t *data, size_t len, const uint8_t mask_key[4])
{
    if (!data || !mask_key) return;
    for (size_t i = 0; i < len; i++)
        data[i] ^= mask_key[i % 4];
}

websocket_frame_err_t websocket_frame_parse(const uint8_t *data, size_t len, websocket_frame_t *frame)
{
    if (!data || !frame)
        return WEBSOCKET_FRAME_ERR_PARAM;

    if (len < 2)
        return WEBSOCKET_FRAME_ERR_INCOMPLETE;

    frame->fin = (data[0] & 0x80) != 0;
    frame->opcode = (websocket_frame_opcode_t)(data[0] & 0x0F);
    frame->masked = (data[1] & 0x80) != 0;

    if ((data[0] & 0x70) != 0)
        return WEBSOCKET_FRAME_ERR_PROTOCOL;

    uint8_t op = (uint8_t)frame->opcode;
    if ((op >= 3 && op <= 7) || (op >= 0x0B))
        return WEBSOCKET_FRAME_ERR_PROTOCOL;

    bool is_control = (frame->opcode & 0x08) != 0;
    uint64_t payload_len;
    size_t hdr_len = 2;
    uint8_t len7 = data[1] & 0x7F;

    if (is_control && (!frame->fin || len7 > 125))
        return WEBSOCKET_FRAME_ERR_PROTOCOL;

    if (len7 <= 125) {
        payload_len = len7;
    } else if (len7 == 126) {
        if (len < 4) return WEBSOCKET_FRAME_ERR_INCOMPLETE;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        hdr_len = 4;
    } else {
        if (len < 10) return WEBSOCKET_FRAME_ERR_INCOMPLETE;
        /* RFC 6455 s5.2: most significant bit of 64-bit length MUST be 0 */
        if (data[2] & 0x80) return WEBSOCKET_FRAME_ERR_PROTOCOL;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | data[2 + i];
        hdr_len = 10;
    }

    if (frame->masked) {
        if (len < hdr_len + 4) return WEBSOCKET_FRAME_ERR_INCOMPLETE;
        memcpy(frame->mask_key, &data[hdr_len], 4);
        hdr_len += 4;
    } else {
        memset(frame->mask_key, 0, 4);
    }

    if (payload_len > (uint64_t)(SIZE_MAX - hdr_len))
        return WEBSOCKET_FRAME_ERR_OVERFLOW;
    if (len < hdr_len + (size_t)payload_len)
        return WEBSOCKET_FRAME_ERR_INCOMPLETE;

    frame->payload = &data[hdr_len];
    frame->payload_len = (size_t)payload_len;
    frame->frame_len = hdr_len + (size_t)payload_len;
    ENSURE(frame->frame_len <= len);
    return WEBSOCKET_FRAME_OK;
}

websocket_frame_err_t websocket_frame_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                        bool fin, websocket_frame_opcode_t opcode,
                        const uint8_t *mask_key,
                        const uint8_t *payload, size_t payload_len)
{
    if (!buf || !out_len)
        return WEBSOCKET_FRAME_ERR_PARAM;
    if (!payload && payload_len > 0)
        return WEBSOCKET_FRAME_ERR_PARAM;

    /* RFC 6455 s5.5: control frames are short and never fragmented */
    if (((uint8_t)opcode & 0x08) != 0 && (!fin || payload_len > 125))
        return WEBSOCKET_FRAME_ERR_PROTOCOL;

    size_t hdr_len = 2;
    if (payload_len > 125 && payload_len <= 65535)
        hdr_len = 4;
    else if (payload_len > 65535)
        hdr_len = 10;

    if (mask_key)
        hdr_len += 4;

    if (payload_len > SIZE_MAX - hdr_len)
        return WEBSOCKET_FRAME_ERR_OVERFLOW;
    size_t total = hdr_len + payload_len;
    if (total > buf_len)
        return WEBSOCKET_FRAME_ERR_OVERFLOW;

    buf[0] = (uint8_t)((fin ? 0x80 : 0x00) | ((uint8_t)opcode & 0x0F));

    uint8_t mask_bit = mask_key ? 0x80 : 0x00;
    size_t idx = 1;

    if (payload_len <= 125) {
        buf[idx++] = (uint8_t)(mask_bit | (uint8_t)payload_len);
    } else if (payload_len <= 65535) {
        buf[idx++] = (uint8_t)(mask_bit | 126);
        buf[idx++] = (uint8_t)(payload_len >> 8);
        buf[idx++] = (uint8_t)(payload_len & 0xFF);
    } else {
        buf[idx++] = (uint8_t)(mask_bit | 127);
        {
            uint64_t plen64 = (uint64_t)payload_len;
            for (int i = 7; i >= 0; i--)
                buf[idx++] = (uint8_t)((plen64 >> (i * 8)) & 0xFF);
        }
    }

    if (mask_key) {
        memcpy(&buf[idx], mask_key, 4);
        idx += 4;
    }

    if (payload && payload_len > 0) {
        memcpy(&buf[idx], payload, payload_len);
        if (mask_key)
            (void)websocket_frame_mask_payload(&buf[idx], payload_len, mask_key);
    }

    *out_len = total;
    ENSURE(total <= buf_len);
    return WEBSOCKET_FRAME_OK;
}
