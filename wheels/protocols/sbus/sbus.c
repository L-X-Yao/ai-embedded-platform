// SPDX-License-Identifier: MIT
/*
 * sbus -- Futaba S.Bus RC receiver protocol parser and encoder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "sbus.h"
#include <string.h>

/* ---------- parser ---------- */

sbus_err_t sbus_parser_init(sbus_parser_t *parser)
{
    if (!parser) return SBUS_ERR_PARAM;

    memset(parser, 0, sizeof(*parser));
    parser->state = SBUS_PARSE_SEEKING;
    return SBUS_OK;
}

sbus_parse_state_t sbus_parse_byte(sbus_parser_t *parser, uint8_t byte)
{
    if (!parser) return SBUS_PARSE_ERROR;

    switch (parser->state) {
    case SBUS_PARSE_SEEKING:
        if (byte == SBUS_HEADER) {
            parser->buf[0] = byte;
            parser->idx = 1;
            parser->state = SBUS_PARSE_IN_FRAME;
        }
        return parser->state;

    case SBUS_PARSE_IN_FRAME:
        parser->buf[parser->idx] = byte;
        parser->idx++;
        if (parser->idx >= SBUS_FRAME_LEN) {
            /* Check footer */
            if (parser->buf[SBUS_FRAME_LEN - 1] == SBUS_FOOTER) {
                parser->state = SBUS_PARSE_COMPLETE;
            } else {
                /* Bad footer -- reset to seeking.
                 * But the byte we just received might be a header for the
                 * next frame, so check. */
                parser->state = SBUS_PARSE_SEEKING;
                if (byte == SBUS_HEADER) {
                    parser->buf[0] = byte;
                    parser->idx = 1;
                    parser->state = SBUS_PARSE_IN_FRAME;
                }
                return SBUS_PARSE_ERROR;
            }
        }
        return parser->state;

    case SBUS_PARSE_COMPLETE:
        /* Previous frame not consumed -- restart seeking */
        parser->state = SBUS_PARSE_SEEKING;
        if (byte == SBUS_HEADER) {
            parser->buf[0] = byte;
            parser->idx = 1;
            parser->state = SBUS_PARSE_IN_FRAME;
        }
        return parser->state;

    case SBUS_PARSE_ERROR:
        /* Recover: go back to seeking */
        parser->state = SBUS_PARSE_SEEKING;
        if (byte == SBUS_HEADER) {
            parser->buf[0] = byte;
            parser->idx = 1;
            parser->state = SBUS_PARSE_IN_FRAME;
        }
        return parser->state;

    default:
        parser->state = SBUS_PARSE_SEEKING;
        return SBUS_PARSE_ERROR;
    }
}

/* ---------- decode ---------- */

sbus_err_t sbus_decode(const uint8_t frame[SBUS_FRAME_LEN], sbus_frame_t *out)
{
    if (!frame || !out) return SBUS_ERR_PARAM;

    if (frame[0] != SBUS_HEADER) return SBUS_ERR_FORMAT;
    if (frame[SBUS_FRAME_LEN - 1] != SBUS_FOOTER) return SBUS_ERR_FORMAT;

    memset(out, 0, sizeof(*out));

    /*
     * Channels are packed as 16 x 11-bit values in bytes 1..22 (176 bits
     * = 22 bytes).  Bits are packed LSB-first in little-endian order:
     *   CH1  = bits[ 0:10]   -> byte1[0:7] | byte2[0:2]
     *   CH2  = bits[11:21]   -> byte2[3:7] | byte3[0:5]
     *   ...
     *
     * Generic extraction: for channel i (0-based), the bit offset is
     * i*11. Starting byte = 1 + bitOffset/8, starting bit within that
     * byte = bitOffset%8. We need up to 3 bytes to extract 11 bits.
     */
    const uint8_t *payload = &frame[1];

    for (int ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
        int bit_offset = ch * 11;
        int byte_idx = bit_offset / 8;
        int bit_idx  = bit_offset % 8;

        /* Extract up to 24 bits from 3 consecutive bytes */
        uint32_t word = (uint32_t)payload[byte_idx];
        if (byte_idx + 1 < 22) {
            word |= (uint32_t)payload[byte_idx + 1] << 8;
        }
        if (byte_idx + 2 < 22) {
            word |= (uint32_t)payload[byte_idx + 2] << 16;
        }

        out->channels[ch] = (uint16_t)((word >> bit_idx) & 0x7FF);
    }

    /* Flags byte (byte 23) */
    uint8_t flags = frame[23];
    out->ch17       = (flags & SBUS_FLAG_CH17)       != 0;
    out->ch18       = (flags & SBUS_FLAG_CH18)       != 0;
    out->frame_lost = (flags & SBUS_FLAG_FRAME_LOST) != 0;
    out->failsafe   = (flags & SBUS_FLAG_FAILSAFE)   != 0;

    return SBUS_OK;
}

/* ---------- encode ---------- */

sbus_err_t sbus_encode(const sbus_frame_t *frame, uint8_t buf[SBUS_FRAME_LEN])
{
    if (!frame || !buf) return SBUS_ERR_PARAM;

    memset(buf, 0, SBUS_FRAME_LEN);

    buf[0] = SBUS_HEADER;

    /* Pack 16 channels x 11 bits into bytes 1..22 */
    uint8_t *payload = &buf[1];

    for (int ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
        uint16_t val = frame->channels[ch] & 0x7FF;
        int bit_offset = ch * 11;
        int byte_idx = bit_offset / 8;
        int bit_idx  = bit_offset % 8;

        payload[byte_idx]     |= (uint8_t)((uint32_t)val << bit_idx);
        if (byte_idx + 1 < 22) {
            payload[byte_idx + 1] |= (uint8_t)((uint32_t)val >> (8 - bit_idx));
        }
        if (bit_idx > 5 && byte_idx + 2 < 22) {
            payload[byte_idx + 2] |= (uint8_t)((uint32_t)val >> (16 - bit_idx));
        }
    }

    /* Flags byte */
    uint8_t flags = 0;
    if (frame->ch17)       flags |= SBUS_FLAG_CH17;
    if (frame->ch18)       flags |= SBUS_FLAG_CH18;
    if (frame->frame_lost) flags |= SBUS_FLAG_FRAME_LOST;
    if (frame->failsafe)   flags |= SBUS_FLAG_FAILSAFE;
    buf[23] = flags;

    buf[24] = SBUS_FOOTER;

    return SBUS_OK;
}

/* ---------- parser buffer access ---------- */

const uint8_t *sbus_parser_get_buf(const sbus_parser_t *parser)
{
    if (!parser) return NULL;
    return parser->buf;
}

/* ---------- utility ---------- */

uint16_t sbus_channel_to_us(uint16_t channel_value)
{
    /* Clamp to RC range */
    if (channel_value < SBUS_RC_MIN) channel_value = SBUS_RC_MIN;
    if (channel_value > SBUS_RC_MAX) channel_value = SBUS_RC_MAX;

    /*
     * Linear map: 172..1811 -> 1000..2000
     * us = 1000 + (val - 172) * 1000 / (1811 - 172)
     *    = 1000 + (val - 172) * 1000 / 1639
     */
    uint32_t numerator = (uint32_t)(channel_value - SBUS_RC_MIN) * (SBUS_US_MAX - SBUS_US_MIN);
    uint16_t us = (uint16_t)(SBUS_US_MIN + numerator / (SBUS_RC_MAX - SBUS_RC_MIN));
    return us;
}

uint16_t sbus_us_to_channel(uint16_t us)
{
    /* Clamp to us range */
    if (us < SBUS_US_MIN) us = SBUS_US_MIN;
    if (us > SBUS_US_MAX) us = SBUS_US_MAX;

    /*
     * Inverse: 1000..2000 -> 172..1811
     * val = 172 + (us - 1000) * 1639 / 1000
     */
    uint32_t numerator = (uint32_t)(us - SBUS_US_MIN) * (SBUS_RC_MAX - SBUS_RC_MIN);
    uint16_t val = (uint16_t)(SBUS_RC_MIN + numerator / (SBUS_US_MAX - SBUS_US_MIN));
    return val;
}
