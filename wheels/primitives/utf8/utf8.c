// SPDX-License-Identifier: MIT
/*
 * utf8 — UTF-8 validation, decoding, and length counting
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "utf8.h"

uint8_t utf8_byte_length(uint8_t lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

static bool is_continuation(uint8_t b)
{
    return (b & 0xC0) == 0x80;
}

utf8_err_t utf8_decode(const uint8_t *data, size_t len,
                        uint32_t *codepoint, size_t *out_len)
{
    if (!data || !codepoint || !out_len)
        return UTF8_ERR_PARAM;
    if (len == 0)
        return UTF8_ERR_SHORT;

    uint8_t n = utf8_byte_length(data[0]);
    if (n == 0)
        return UTF8_ERR_INVALID;
    if (n > len)
        return UTF8_ERR_SHORT;

    uint32_t cp;
    switch (n) {
    case 1: cp = data[0]; break;
    case 2:
        if (!is_continuation(data[1])) return UTF8_ERR_INVALID;
        cp = ((uint32_t)(data[0] & 0x1F) << 6) | (data[1] & 0x3F);
        if (cp < 0x80) return UTF8_ERR_INVALID;
        break;
    case 3:
        if (!is_continuation(data[1]) || !is_continuation(data[2]))
            return UTF8_ERR_INVALID;
        cp = ((uint32_t)(data[0] & 0x0F) << 12) |
             ((uint32_t)(data[1] & 0x3F) << 6) | (data[2] & 0x3F);
        if (cp < 0x800) return UTF8_ERR_INVALID;
        if (cp >= 0xD800 && cp <= 0xDFFF) return UTF8_ERR_INVALID;
        break;
    case 4:
        if (!is_continuation(data[1]) || !is_continuation(data[2]) ||
            !is_continuation(data[3]))
            return UTF8_ERR_INVALID;
        cp = ((uint32_t)(data[0] & 0x07) << 18) |
             ((uint32_t)(data[1] & 0x3F) << 12) |
             ((uint32_t)(data[2] & 0x3F) << 6) | (data[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) return UTF8_ERR_INVALID;
        break;
    default:
        return UTF8_ERR_INVALID;
    }

    *codepoint = cp;
    *out_len = n;
    return UTF8_OK;
}

utf8_err_t utf8_encode(uint32_t codepoint, uint8_t *buf, size_t cap,
                        size_t *out_len)
{
    if (!buf || !out_len)
        return UTF8_ERR_PARAM;
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
        return UTF8_ERR_INVALID;
    if (codepoint > 0x10FFFF)
        return UTF8_ERR_INVALID;

    size_t n;
    if (codepoint < 0x80) {
        n = 1;
        if (cap < n) return UTF8_ERR_SHORT;
        buf[0] = (uint8_t)codepoint;
    } else if (codepoint < 0x800) {
        n = 2;
        if (cap < n) return UTF8_ERR_SHORT;
        buf[0] = (uint8_t)(0xC0 | (codepoint >> 6));
        buf[1] = (uint8_t)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        n = 3;
        if (cap < n) return UTF8_ERR_SHORT;
        buf[0] = (uint8_t)(0xE0 | (codepoint >> 12));
        buf[1] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[2] = (uint8_t)(0x80 | (codepoint & 0x3F));
    } else {
        n = 4;
        if (cap < n) return UTF8_ERR_SHORT;
        buf[0] = (uint8_t)(0xF0 | (codepoint >> 18));
        buf[1] = (uint8_t)(0x80 | ((codepoint >> 12) & 0x3F));
        buf[2] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[3] = (uint8_t)(0x80 | (codepoint & 0x3F));
    }
    *out_len = n;
    return UTF8_OK;
}

utf8_err_t utf8_validate(const uint8_t *data, size_t len)
{
    if (!data && len > 0)
        return UTF8_ERR_PARAM;
    size_t pos = 0;
    while (pos < len) {
        uint32_t cp;
        size_t n;
        utf8_err_t err = utf8_decode(&data[pos], len - pos, &cp, &n);
        if (err != UTF8_OK)
            return err;
        pos += n;
    }
    return UTF8_OK;
}

int32_t utf8_codepoint_count(const uint8_t *data, size_t len)
{
    if (!data && len > 0)
        return -1;
    int32_t count = 0;
    size_t pos = 0;
    while (pos < len) {
        uint32_t cp;
        size_t n;
        if (utf8_decode(&data[pos], len - pos, &cp, &n) != UTF8_OK)
            return -1;
        pos += n;
        count++;
    }
    return count;
}
