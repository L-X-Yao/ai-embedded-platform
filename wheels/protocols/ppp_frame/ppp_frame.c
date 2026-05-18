// SPDX-License-Identifier: MIT
/*
 * ppp_frame -- PPP HDLC-like framing codec (RFC 1662)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ppp_frame.h"
#include "contract.h"
#include <string.h>

/* ---------- FCS-16 lookup table (CRC-16/HDLC, poly 0x8408) ---------- */

static const uint16_t fcs_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78,
};

/* ---------- FCS-16 ---------- */

uint16_t ppp_frame_fcs16(const uint8_t *data, size_t len)
{
    if (!data) return PPP_FCS_INIT;

    uint16_t fcs = PPP_FCS_INIT;
    for (size_t i = 0; i < len; i++) {
        fcs = (uint16_t)((fcs >> 8) ^ fcs_table[(fcs ^ data[i]) & 0xFF]);
    }
    return (uint16_t)(fcs ^ 0xFFFF);
}

/* ---------- byte stuffing helpers ---------- */

/** Check if a byte needs escaping in PPP HDLC framing. */
static bool needs_escape(uint8_t b)
{
    return b == PPP_FLAG || b == PPP_ESCAPE || b < 0x20;
}

/** Write a byte with escaping. Returns number of bytes written (1 or 2),
 *  or 0 if buffer full. */
static size_t write_escaped(uint8_t *dst, size_t pos, size_t cap, uint8_t b)
{
    if (needs_escape(b)) {
        if (pos + 2 > cap) return 0;
        dst[pos]     = PPP_ESCAPE;
        dst[pos + 1] = (uint8_t)(b ^ PPP_XOR_MASK);
        return 2;
    }
    if (pos + 1 > cap) return 0;
    dst[pos] = b;
    return 1;
}

/* ---------- encode ---------- */

ppp_frame_err_t ppp_frame_encode(uint16_t protocol,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *dst, size_t dst_cap,
                                  size_t *out_len)
{
    if (!dst || !out_len) return PPP_FRAME_ERR_PARAM;
    if (payload_len > 0 && !payload) return PPP_FRAME_ERR_PARAM;

    /* Build unescaped frame content for FCS: addr + control + protocol + payload */
    /* We compute FCS over the unescaped data */
    uint8_t header[4];
    header[0] = PPP_ADDR;
    header[1] = PPP_CONTROL;
    header[2] = (uint8_t)(protocol >> 8);
    header[3] = (uint8_t)(protocol & 0xFF);

    /* Compute FCS over header + payload */
    uint16_t fcs = PPP_FCS_INIT;
    for (size_t i = 0; i < 4; i++) {
        fcs = (uint16_t)((fcs >> 8) ^ fcs_table[(fcs ^ header[i]) & 0xFF]);
    }
    for (size_t i = 0; i < payload_len; i++) {
        fcs = (uint16_t)((fcs >> 8) ^ fcs_table[(fcs ^ payload[i]) & 0xFF]);
    }
    fcs = (uint16_t)(fcs ^ 0xFFFF);

    /* Now build the escaped frame */
    size_t pos = 0;

    /* Opening flag */
    if (pos >= dst_cap) return PPP_FRAME_ERR_OVERFLOW;
    dst[pos++] = PPP_FLAG;

    /* Address, control, protocol (escaped) */
    for (size_t i = 0; i < 4; i++) {
        size_t n = write_escaped(dst, pos, dst_cap, header[i]);
        if (n == 0) return PPP_FRAME_ERR_OVERFLOW;
        pos += n;
    }

    /* Payload (escaped) */
    for (size_t i = 0; i < payload_len; i++) {
        size_t n = write_escaped(dst, pos, dst_cap, payload[i]);
        if (n == 0) return PPP_FRAME_ERR_OVERFLOW;
        pos += n;
    }

    /* FCS (little-endian, escaped) */
    uint8_t fcs_lo = (uint8_t)(fcs & 0xFF);
    uint8_t fcs_hi = (uint8_t)(fcs >> 8);

    size_t n = write_escaped(dst, pos, dst_cap, fcs_lo);
    if (n == 0) return PPP_FRAME_ERR_OVERFLOW;
    pos += n;

    n = write_escaped(dst, pos, dst_cap, fcs_hi);
    if (n == 0) return PPP_FRAME_ERR_OVERFLOW;
    pos += n;

    /* Closing flag */
    if (pos >= dst_cap) return PPP_FRAME_ERR_OVERFLOW;
    dst[pos++] = PPP_FLAG;

    *out_len = pos;
    ENSURE(pos <= dst_cap);
    return PPP_FRAME_OK;
}

/* ---------- decode ---------- */

ppp_frame_err_t ppp_frame_decode(const uint8_t *src, size_t src_len,
                                  uint16_t *protocol,
                                  uint8_t *payload_buf, size_t payload_cap,
                                  size_t *payload_len)
{
    if (!src || !protocol || !payload_buf || !payload_len)
        return PPP_FRAME_ERR_PARAM;

    /* Must have at least opening and closing flags */
    if (src_len < 2) return PPP_FRAME_ERR_SHORT;
    if (src[0] != PPP_FLAG || src[src_len - 1] != PPP_FLAG)
        return PPP_FRAME_ERR_FORMAT;

    /* De-stuff into temporary buffer on stack.
     * Max unstuffed = src_len (no expansion possible in reverse). */
    uint8_t unstuffed[1500];
    size_t ulen = 0;
    bool escaped = false;

    for (size_t i = 1; i < src_len - 1; i++) {
        if (src[i] == PPP_FLAG) {
            /* Unexpected flag in middle */
            return PPP_FRAME_ERR_FORMAT;
        }
        if (escaped) {
            if (ulen >= sizeof(unstuffed)) return PPP_FRAME_ERR_OVERFLOW;
            unstuffed[ulen++] = (uint8_t)(src[i] ^ PPP_XOR_MASK);
            escaped = false;
        } else if (src[i] == PPP_ESCAPE) {
            escaped = true;
        } else {
            if (ulen >= sizeof(unstuffed)) return PPP_FRAME_ERR_OVERFLOW;
            unstuffed[ulen++] = src[i];
        }
    }

    /* Trailing escape without data */
    if (escaped) return PPP_FRAME_ERR_FORMAT;

    /* Need at least: addr(1) + control(1) + protocol(2) + FCS(2) = 6 */
    if (ulen < 6) return PPP_FRAME_ERR_SHORT;

    /* Verify FCS: compute over all unstuffed bytes except the last 2 (FCS itself) */
    uint16_t fcs = PPP_FCS_INIT;
    for (size_t i = 0; i < ulen - 2; i++) {
        fcs = (uint16_t)((fcs >> 8) ^ fcs_table[(fcs ^ unstuffed[i]) & 0xFF]);
    }
    fcs = (uint16_t)(fcs ^ 0xFFFF);

    uint16_t rx_fcs = (uint16_t)(((uint16_t)unstuffed[ulen - 1] << 8) |
                      (uint16_t)unstuffed[ulen - 2]);
    if (fcs != rx_fcs) return PPP_FRAME_ERR_FCS;

    /* Verify address and control */
    if (unstuffed[0] != PPP_ADDR || unstuffed[1] != PPP_CONTROL)
        return PPP_FRAME_ERR_FORMAT;

    /* Extract protocol */
    *protocol = (uint16_t)(((uint16_t)unstuffed[2] << 8) | (uint16_t)unstuffed[3]);

    /* Extract payload (between protocol and FCS) */
    size_t plen = ulen - 6; /* subtract addr+ctrl+proto+fcs */
    if (plen > payload_cap) return PPP_FRAME_ERR_OVERFLOW;

    if (plen > 0) {
        memcpy(payload_buf, &unstuffed[4], plen);
    }
    *payload_len = plen;

    return PPP_FRAME_OK;
}
