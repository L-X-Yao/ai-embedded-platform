// SPDX-License-Identifier: MIT
/*
 * varint — variable-length integer encoding (LEB128 / protobuf)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "varint.h"

varint_err_t varint_encode_u64(uint64_t value, uint8_t *buf, size_t cap,
                                size_t *out_len)
{
    if (!buf || !out_len)
        return VARINT_ERR_PARAM;

    size_t i = 0;
    do {
        if (i >= cap)
            return VARINT_ERR_OVERFLOW;
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buf[i++] = byte;
    } while (value != 0);

    *out_len = i;
    return VARINT_OK;
}

varint_err_t varint_decode_u64(const uint8_t *buf, size_t len,
                                uint64_t *value, size_t *out_len)
{
    if (!buf || !value || !out_len)
        return VARINT_ERR_PARAM;

    uint64_t result = 0;
    size_t i = 0;
    unsigned shift = 0;

    do {
        if (i >= len)
            return VARINT_ERR_SHORT;
        if (shift >= 64)
            return VARINT_ERR_OVERFLOW;
        uint64_t byte = buf[i];
        /* The tenth byte only has room for bit 63: the rest would be lost */
        if (shift == 63 && (byte & 0x7E))
            return VARINT_ERR_OVERFLOW;
        result |= (byte & 0x7F) << shift;
        shift += 7;
        i++;
    } while (buf[i - 1] & 0x80);

    *value = result;
    *out_len = i;
    return VARINT_OK;
}

static uint64_t zigzag_encode(int64_t n)
{
    return ((uint64_t)n << 1) ^ (uint64_t)(n >> 63);
}

static int64_t zigzag_decode(uint64_t n)
{
    return (int64_t)((n >> 1) ^ -(int64_t)(n & 1));
}

varint_err_t varint_encode_s64(int64_t value, uint8_t *buf, size_t cap,
                                size_t *out_len)
{
    return varint_encode_u64(zigzag_encode(value), buf, cap, out_len);
}

varint_err_t varint_decode_s64(const uint8_t *buf, size_t len,
                                int64_t *value, size_t *out_len)
{
    if (!value)
        return VARINT_ERR_PARAM;
    uint64_t uval;
    varint_err_t err = varint_decode_u64(buf, len, &uval, out_len);
    if (err != VARINT_OK)
        return err;
    *value = zigzag_decode(uval);
    return VARINT_OK;
}

size_t varint_encoded_size_u64(uint64_t value)
{
    size_t n = 1;
    while (value > 0x7F) {
        value >>= 7;
        n++;
    }
    return n;
}
