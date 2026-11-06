// SPDX-License-Identifier: MIT
/*
 * hdlc -- HDLC frame encoding/decoding with byte stuffing and FCS-16
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "hdlc.h"
#include "contract.h"
#include <string.h>

/* ---------- FCS-16 (CRC-16-CCITT reflected, poly 0x8408) ---------- */

uint16_t hdlc_fcs16(const uint8_t *data, size_t len)
{
    if (!data && len > 0) return HDLC_FCS_INIT;

    uint16_t fcs = HDLC_FCS_INIT;
    for (size_t i = 0; i < len; i++) {
        fcs = (uint16_t)(fcs ^ (uint16_t)data[i]);
        for (int j = 0; j < 8; j++) {
            if (fcs & 1u) {
                fcs = (uint16_t)((fcs >> 1) ^ 0x8408u);
            } else {
                fcs >>= 1;
            }
        }
    }
    return (uint16_t)(fcs ^ 0xFFFFu);
}

/* ---------- byte stuffing helper ---------- */

/** Write one byte with HDLC byte stuffing.
 *  Returns number of bytes written (1 or 2), or 0 if buffer full. */
static size_t write_stuffed(uint8_t *dst, size_t pos, size_t cap, uint8_t b)
{
    if (b == HDLC_FLAG || b == HDLC_ESCAPE) {
        if (pos + 2 > cap) return 0;
        dst[pos]     = HDLC_ESCAPE;
        dst[pos + 1] = (uint8_t)(b ^ HDLC_XOR_MASK);
        return 2;
    }
    if (pos + 1 > cap) return 0;
    dst[pos] = b;
    return 1;
}

/* ---------- encode ---------- */

hdlc_err_t hdlc_encode(const uint8_t *data, size_t data_len,
                        uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!out || !out_len) return HDLC_ERR_PARAM;
    if (data_len > 0 && !data) return HDLC_ERR_PARAM;

    /* Compute FCS-16 over the raw data */
    uint16_t fcs = hdlc_fcs16(data, data_len);

    size_t pos = 0;

    /* Opening flag */
    if (pos >= out_cap) return HDLC_ERR_OVERFLOW;
    out[pos++] = HDLC_FLAG;

    /* Data bytes (stuffed) */
    for (size_t i = 0; i < data_len; i++) {
        size_t n = write_stuffed(out, pos, out_cap, data[i]);
        if (n == 0) return HDLC_ERR_OVERFLOW;
        pos += n;
    }

    /* FCS (little-endian, stuffed) */
    uint8_t fcs_lo = (uint8_t)(fcs & 0xFFu);
    uint8_t fcs_hi = (uint8_t)(fcs >> 8);

    size_t n = write_stuffed(out, pos, out_cap, fcs_lo);
    if (n == 0) return HDLC_ERR_OVERFLOW;
    pos += n;

    n = write_stuffed(out, pos, out_cap, fcs_hi);
    if (n == 0) return HDLC_ERR_OVERFLOW;
    pos += n;

    /* Closing flag */
    if (pos >= out_cap) return HDLC_ERR_OVERFLOW;
    out[pos++] = HDLC_FLAG;

    *out_len = pos;
    ENSURE(pos <= out_cap);
    return HDLC_OK;
}

/* ---------- parser ---------- */

hdlc_err_t hdlc_parser_init(hdlc_parser_t *parser, uint8_t *buf, size_t capacity)
{
    if (!parser || !buf || capacity == 0) return HDLC_ERR_PARAM;

    parser->buf      = buf;
    parser->capacity = capacity;
    parser->idx      = 0;
    parser->state    = HDLC_STATE_IDLE;
    return HDLC_OK;
}

hdlc_state_t hdlc_parser_feed(hdlc_parser_t *parser, uint8_t byte)
{
    if (!parser) return HDLC_STATE_ERROR;

    /* One flag can close a frame and open the next (RFC 1662 section 3.1),
     * so whatever follows a complete frame belongs to a new one. */
    if (parser->state == HDLC_STATE_COMPLETE) {
        parser->idx   = 0;
        parser->state = HDLC_STATE_ACTIVE;
    }

    switch (parser->state) {
    case HDLC_STATE_IDLE:
        if (byte == HDLC_FLAG) {
            parser->idx   = 0;
            parser->state = HDLC_STATE_ACTIVE;
        }
        /* Ignore non-flag bytes while idle (garbage before frame) */
        return parser->state;

    case HDLC_STATE_ACTIVE:
        if (byte == HDLC_FLAG) {
            if (parser->idx > 0) {
                /* Closing flag -- frame complete */
                parser->state = HDLC_STATE_COMPLETE;
            }
            /* If idx==0, consecutive flags: stay ACTIVE, reset */
            return parser->state;
        }
        if (byte == HDLC_ESCAPE) {
            parser->state = HDLC_STATE_ESCAPE;
            return parser->state;
        }
        /* Normal byte -- store unstuffed */
        if (parser->idx >= parser->capacity) {
            parser->state = HDLC_STATE_ERROR;
            return HDLC_STATE_ERROR;
        }
        parser->buf[parser->idx++] = byte;
        return parser->state;

    case HDLC_STATE_ESCAPE:
        /* Flag after escape is an abort per RFC 1662 -- discard frame */
        if (byte == HDLC_FLAG) {
            parser->idx   = 0;
            parser->state = HDLC_STATE_ACTIVE;
            return parser->state;
        }
        /* The byte after escape is XOR'd to recover original */
        {
            uint8_t decoded = (uint8_t)(byte ^ HDLC_XOR_MASK);
            if (parser->idx >= parser->capacity) {
                parser->state = HDLC_STATE_ERROR;
                return HDLC_STATE_ERROR;
            }
            parser->buf[parser->idx++] = decoded;
            parser->state = HDLC_STATE_ACTIVE;
        }
        return parser->state;

    case HDLC_STATE_ERROR:
        /* Recover: look for a new flag */
        if (byte == HDLC_FLAG) {
            parser->idx   = 0;
            parser->state = HDLC_STATE_ACTIVE;
        } else {
            parser->state = HDLC_STATE_IDLE;
        }
        return parser->state;

    default:
        parser->state = HDLC_STATE_IDLE;
        return HDLC_STATE_ERROR;
    }
}

hdlc_err_t hdlc_parser_get_frame(const hdlc_parser_t *parser,
                                  uint8_t *data, size_t data_cap,
                                  size_t *data_len)
{
    if (!parser || !data || !data_len) return HDLC_ERR_PARAM;
    if (parser->state != HDLC_STATE_COMPLETE) return HDLC_ERR_FORMAT;

    /* Parser buffer contains unstuffed data + FCS(2 bytes) */
    if (parser->idx < 2) return HDLC_ERR_FORMAT;

    /* Verify FCS: compute over data (excluding FCS bytes) */
    size_t payload_len = parser->idx - 2;
    uint16_t computed = hdlc_fcs16(parser->buf, payload_len);

    uint16_t rx_fcs = (uint16_t)(((uint16_t)parser->buf[parser->idx - 1] << 8) |
                      (uint16_t)parser->buf[parser->idx - 2]);

    if (computed != rx_fcs) return HDLC_ERR_FCS;

    if (payload_len > data_cap) return HDLC_ERR_OVERFLOW;

    if (payload_len > 0) {
        memcpy(data, parser->buf, payload_len);
    }
    *data_len = payload_len;

    return HDLC_OK;
}

void hdlc_parser_reset(hdlc_parser_t *parser)
{
    if (!parser) return;
    parser->idx   = 0;
    parser->state = HDLC_STATE_IDLE;
}
