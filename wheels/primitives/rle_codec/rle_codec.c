// SPDX-License-Identifier: MIT
/*
 * rle_codec — run-length encoding / decoding
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: stateless
 */

#include "rle_codec.h"
#include "contract.h"

rle_codec_err_t rle_codec_encode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len)
{
    if (!src || !dst || !out_len) {
        return RLE_CODEC_ERR_PARAM;
    }

    /* Empty input → empty output */
    if (src_len == 0) {
        *out_len = 0;
        return RLE_CODEC_OK;
    }

    size_t di = 0; /* dst index */
    size_t si = 0; /* src index */

    while (si < src_len) {
        uint8_t val = src[si];
        size_t  run = 1;

        /* Count consecutive identical bytes, max 255 per pair */
        while (si + run < src_len && src[si + run] == val && run < 255) {
            run++;
        }

        /* Need 2 bytes in dst for this pair */
        if (di + 2 > dst_cap) {
            *out_len = 0;
            return RLE_CODEC_ERR_OVERFLOW;
        }

        dst[di]     = (uint8_t)run;
        dst[di + 1] = val;
        di += 2;
        si += run;
    }

    *out_len = di;
    ENSURE(di <= dst_cap);
    ENSURE(di % 2 == 0);
    return RLE_CODEC_OK;
}

rle_codec_err_t rle_codec_decode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len)
{
    if (!src || !dst || !out_len) {
        return RLE_CODEC_ERR_PARAM;
    }

    /* Encoded data must be pairs → even length */
    if (src_len % 2 != 0) {
        return RLE_CODEC_ERR_PARAM;
    }

    /* Empty input → empty output */
    if (src_len == 0) {
        *out_len = 0;
        return RLE_CODEC_OK;
    }

    size_t di = 0;

    for (size_t si = 0; si < src_len; si += 2) {
        uint8_t count = src[si];
        uint8_t val   = src[si + 1];

        /* count == 0 is invalid in our format but handle gracefully */
        if (count == 0) {
            continue;
        }

        if (di + count > dst_cap) {
            *out_len = 0;
            return RLE_CODEC_ERR_OVERFLOW;
        }

        for (uint8_t i = 0; i < count; i++) {
            dst[di++] = val;
        }
    }

    *out_len = di;
    ENSURE(di <= dst_cap);
    return RLE_CODEC_OK;
}

size_t rle_codec_encode_bound(size_t src_len)
{
    /* Worst case: no runs at all → every byte becomes [1][val] = 2 bytes */
    return src_len * 2;
}

size_t rle_codec_decode_bound(size_t src_len)
{
    /* Must be even (pairs) */
    if (src_len % 2 != 0) {
        return 0;
    }
    /* Worst case: every pair is [255][val] */
    return (src_len / 2) * 255;
}
