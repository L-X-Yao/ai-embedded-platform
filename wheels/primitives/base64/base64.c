// SPDX-License-Identifier: MIT
/*
 * base64 — Base64 encoder/decoder for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * RFC 4648 standard alphabet. Zero allocation.
 */

#include "base64.h"
#include "contract.h"
#include <stddef.h>

/* ---- Lookup tables ---------------------------------------------------- */

static const char enc_table[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * Decode table: maps ASCII 0..127 to 6-bit value.
 * 0xFF = invalid, 0xFE = padding ('=').
 */
static const uint8_t dec_table[128] = {
    /* 0x00-0x0F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 0x10-0x1F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 0x20-0x2F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                    0xFF,0xFF,0xFF,  62,0xFF,0xFF,0xFF,  63,
    /* 0x30-0x3F */   52,  53,  54,  55,  56,  57,  58,  59,
                      60,  61,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,
    /* 0x40-0x4F */ 0xFF,   0,   1,   2,   3,   4,   5,   6,
                       7,   8,   9,  10,  11,  12,  13,  14,
    /* 0x50-0x5F */   15,  16,  17,  18,  19,  20,  21,  22,
                      23,  24,  25,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 0x60-0x6F */ 0xFF,  26,  27,  28,  29,  30,  31,  32,
                      33,  34,  35,  36,  37,  38,  39,  40,
    /* 0x70-0x7F */   41,  42,  43,  44,  45,  46,  47,  48,
                      49,  50,  51,0xFF,0xFF,0xFF,0xFF,0xFF,
};

/* ---- Size helpers ----------------------------------------------------- */

uint16_t base64_encoded_size(uint16_t data_len)
{
    /* ceil(data_len / 3) * 4 */
    return (uint16_t)(((uint32_t)data_len + 2u) / 3u * 4u);
}

uint16_t base64_decoded_size(uint16_t b64_len)
{
    /* Upper bound: 3 bytes per 4 chars (may over-estimate by up to 2) */
    return (uint16_t)((uint32_t)b64_len / 4u * 3u);
}

/* ---- Encode ----------------------------------------------------------- */

base64_err_t base64_encode(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return BASE64_ERR_PARAM;
    }
    if (data == NULL && data_len != 0u) {
        return BASE64_ERR_PARAM;
    }

    /* 32-bit: past 49149 input bytes the size no longer fits uint16_t. */
    uint32_t needed = ((uint32_t)data_len + 2u) / 3u * 4u;
    if ((uint32_t)*out_len < needed + 1u) {   /* +1 for NUL */
        return BASE64_ERR_OVERFLOW;
    }

    uint16_t i = 0;     /* input index  */
    uint16_t o = 0;     /* output index */

    /* Process full 3-byte groups */
    while (i + 3u <= data_len) {
        uint32_t triple = ((uint32_t)data[i] << 16)
                        | ((uint32_t)data[i + 1u] << 8)
                        |  (uint32_t)data[i + 2u];
        out[o]      = enc_table[(triple >> 18) & 0x3Fu];
        out[o + 1u] = enc_table[(triple >> 12) & 0x3Fu];
        out[o + 2u] = enc_table[(triple >>  6) & 0x3Fu];
        out[o + 3u] = enc_table[ triple        & 0x3Fu];
        i = (uint16_t)(i + 3u);
        o = (uint16_t)(o + 4u);
    }

    /* Handle remainder (1 or 2 bytes) */
    uint16_t rem = (uint16_t)(data_len - i);
    if (rem == 1u) {
        uint32_t val = (uint32_t)data[i] << 16;
        out[o]      = enc_table[(val >> 18) & 0x3Fu];
        out[o + 1u] = enc_table[(val >> 12) & 0x3Fu];
        out[o + 2u] = '=';
        out[o + 3u] = '=';
        o = (uint16_t)(o + 4u);
    } else if (rem == 2u) {
        uint32_t val = ((uint32_t)data[i] << 16)
                     | ((uint32_t)data[i + 1u] << 8);
        out[o]      = enc_table[(val >> 18) & 0x3Fu];
        out[o + 1u] = enc_table[(val >> 12) & 0x3Fu];
        out[o + 2u] = enc_table[(val >>  6) & 0x3Fu];
        out[o + 3u] = '=';
        o = (uint16_t)(o + 4u);
    }

    out[o] = '\0';
    *out_len = o;
    ENSURE(o == needed);
    return BASE64_OK;
}

/* ---- Decode ----------------------------------------------------------- */

base64_err_t base64_decode(const char *b64, uint16_t b64_len,
                           uint8_t *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return BASE64_ERR_PARAM;
    }
    if (b64 == NULL && b64_len != 0u) {
        return BASE64_ERR_PARAM;
    }
    if (b64_len == 0u) {
        *out_len = 0;
        return BASE64_OK;
    }

    /* Strip trailing padding */
    uint16_t len = b64_len;
    while (len > 0u && b64[len - 1u] == '=') {
        len--;
    }

    /* Compute exact output size from non-padding character count */
    uint16_t tail = (uint16_t)(len % 4u);
    uint16_t out_bytes;

    if (tail == 0u) {
        out_bytes = (uint16_t)(len / 4u * 3u);
    } else if (tail == 2u) {
        out_bytes = (uint16_t)(len / 4u * 3u + 1u);
    } else if (tail == 3u) {
        out_bytes = (uint16_t)(len / 4u * 3u + 2u);
    } else {
        return BASE64_ERR_INVALID;   /* 1 char is never valid */
    }

    if (*out_len < out_bytes) {
        return BASE64_ERR_OVERFLOW;
    }

    /* Decode full 4-char groups */
    uint16_t si = 0;     /* source index */
    uint16_t di = 0;     /* dest index   */

    while (si + 4u <= len) {
        uint32_t accum = 0;
        for (int k = 0; k < 4; k++) {
            uint8_t ch = (uint8_t)b64[si + (uint16_t)k];
            if (ch > 127u) { return BASE64_ERR_INVALID; }
            uint8_t v = dec_table[ch];
            if (v >= 0xFEu) { return BASE64_ERR_INVALID; }
            accum = (accum << 6) | v;
        }
        out[di]      = (uint8_t)(accum >> 16);
        out[di + 1u] = (uint8_t)(accum >> 8);
        out[di + 2u] = (uint8_t)(accum);
        si = (uint16_t)(si + 4u);
        di = (uint16_t)(di + 3u);
    }

    /* Decode tail (2 or 3 chars) */
    if (tail >= 2u) {
        uint32_t accum = 0;
        for (uint16_t k = 0; k < tail; k++) {
            uint8_t ch = (uint8_t)b64[si + k];
            if (ch > 127u) { return BASE64_ERR_INVALID; }
            uint8_t v = dec_table[ch];
            if (v >= 0xFEu) { return BASE64_ERR_INVALID; }
            accum = (accum << 6) | v;
        }
        /* RFC 4648 ss3.5: reject non-zero trailing bits in partial groups.
         * tail=2 → 12 data bits, 8 useful → 4 unused.
         * tail=3 → 18 data bits, 16 useful → 2 unused. */
        uint32_t unused_bits = (tail == 2u) ? 4u : 2u;
        if (accum & ((1u << unused_bits) - 1u)) {
            return BASE64_ERR_INVALID;
        }
        /* Left-align remaining bits */
        accum <<= (4u - tail) * 6u;
        out[di] = (uint8_t)(accum >> 16);
        if (tail == 3u) {
            out[di + 1u] = (uint8_t)(accum >> 8);
        }
    }

    *out_len = out_bytes;
    ENSURE(*out_len <= b64_len);
    return BASE64_OK;
}
