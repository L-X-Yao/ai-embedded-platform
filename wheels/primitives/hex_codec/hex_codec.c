// SPDX-License-Identifier: MIT
/*
 * hex_codec — hex string encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "hex_codec.h"
#include <stddef.h>
#include "contract.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static const char lut_lower[] = "0123456789abcdef";
static const char lut_upper[] = "0123456789ABCDEF";

static hex_err_t hex_encode_impl(const uint8_t *data, uint16_t data_len,
                                 char *out, uint16_t out_size,
                                 const char *lut)
{
    if (data_len > 0 && data == NULL) { return HEX_ERR_PARAM; }
    if (out == NULL)                  { return HEX_ERR_PARAM; }

    /* Guard uint16_t overflow: data_len * 2 + 1 must fit in uint16_t */
    if (data_len > 32767u)            { return HEX_ERR_OVERFLOW; }

    uint16_t needed = (uint16_t)(data_len * 2u + 1u);
    if (out_size < needed)            { return HEX_ERR_OVERFLOW; }

    for (uint16_t i = 0; i < data_len; i++) {
        out[i * 2u]      = lut[(data[i] >> 4) & 0x0Fu];
        out[i * 2u + 1u] = lut[data[i] & 0x0Fu];
    }
    out[data_len * 2u] = '\0';
    ENSURE(out[data_len * 2u] == '\0');
    return HEX_OK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

hex_err_t hex_encode(const uint8_t *data, uint16_t data_len,
                     char *out, uint16_t out_size)
{
    return hex_encode_impl(data, data_len, out, out_size, lut_lower);
}

hex_err_t hex_encode_upper(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t out_size)
{
    return hex_encode_impl(data, data_len, out, out_size, lut_upper);
}

hex_err_t hex_decode(const char *hex, uint16_t hex_len,
                     uint8_t *out, uint16_t *out_len)
{
    if (out_len == NULL)                      { return HEX_ERR_PARAM; }
    if (hex_len > 0 && hex == NULL)           { return HEX_ERR_PARAM; }
    if (hex_len > 0 && out == NULL)           { return HEX_ERR_PARAM; }
    if (hex_len % 2u != 0)                    { return HEX_ERR_ODD_LENGTH; }

    uint16_t decoded = (uint16_t)(hex_len / 2u);
    if (*out_len < decoded)                   { return HEX_ERR_OVERFLOW; }

    for (uint16_t i = 0; i < decoded; i++) {
        int hi = hex_char_to_nibble(hex[i * 2u]);
        int lo = hex_char_to_nibble(hex[i * 2u + 1u]);
        if (hi < 0 || lo < 0)                { return HEX_ERR_INVALID; }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = decoded;
    ENSURE(*out_len == hex_len / 2u);
    return HEX_OK;
}

uint16_t hex_encoded_size(uint16_t data_len)
{
    return (uint16_t)(data_len * 2u);
}

uint16_t hex_decoded_size(uint16_t hex_len)
{
    return (uint16_t)(hex_len / 2u);
}

char hex_nibble_to_char(uint8_t nibble, bool uppercase)
{
    return uppercase ? lut_upper[nibble & 0x0Fu]
                     : lut_lower[nibble & 0x0Fu];
}

int hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}
