// SPDX-License-Identifier: MIT
/*
 * bitstream — bit-level read/write over a byte buffer
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "bitstream.h"

bitstream_err_t bitstream_init(bitstream_t *ctx, uint8_t *buf, size_t cap_bytes)
{
    if (!ctx || !buf)
        return BITSTREAM_ERR_PARAM;
    ctx->buf = buf;
    ctx->cap_bits = cap_bytes * 8;
    ctx->pos = 0;
    return BITSTREAM_OK;
}

bitstream_err_t bitstream_write(bitstream_t *ctx, uint32_t value, uint8_t nbits)
{
    if (!ctx)
        return BITSTREAM_ERR_PARAM;
    if (nbits == 0 || nbits > 32)
        return BITSTREAM_ERR_PARAM;
    if (ctx->pos + nbits > ctx->cap_bits)
        return BITSTREAM_ERR_OVERFLOW;

    for (uint8_t i = 0; i < nbits; i++) {
        size_t byte_idx = ctx->pos / 8;
        uint8_t bit_idx = (uint8_t)(ctx->pos % 8);
        if (value & (1u << i))
            ctx->buf[byte_idx] |= (uint8_t)(1u << bit_idx);
        else
            ctx->buf[byte_idx] &= (uint8_t)~(1u << bit_idx);
        ctx->pos++;
    }

    return BITSTREAM_OK;
}

bitstream_err_t bitstream_read(bitstream_t *ctx, uint8_t nbits, uint32_t *out)
{
    if (!ctx || !out)
        return BITSTREAM_ERR_PARAM;
    if (nbits == 0 || nbits > 32)
        return BITSTREAM_ERR_PARAM;
    if (ctx->pos + nbits > ctx->cap_bits)
        return BITSTREAM_ERR_OVERFLOW;

    uint32_t val = 0;
    for (uint8_t i = 0; i < nbits; i++) {
        size_t byte_idx = ctx->pos / 8;
        uint8_t bit_idx = (uint8_t)(ctx->pos % 8);
        if (ctx->buf[byte_idx] & (1u << bit_idx))
            val |= (1u << i);
        ctx->pos++;
    }

    *out = val;
    return BITSTREAM_OK;
}

size_t bitstream_tell(const bitstream_t *ctx)
{
    if (!ctx)
        return 0;
    return ctx->pos;
}

bitstream_err_t bitstream_seek(bitstream_t *ctx, size_t bit_pos)
{
    if (!ctx)
        return BITSTREAM_ERR_PARAM;
    if (bit_pos > ctx->cap_bits)
        return BITSTREAM_ERR_OVERFLOW;
    ctx->pos = bit_pos;
    return BITSTREAM_OK;
}

bitstream_err_t bitstream_reset(bitstream_t *ctx)
{
    if (!ctx)
        return BITSTREAM_ERR_PARAM;
    ctx->pos = 0;
    return BITSTREAM_OK;
}
