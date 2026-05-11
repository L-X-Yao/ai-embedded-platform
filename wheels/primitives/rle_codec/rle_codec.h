// SPDX-License-Identifier: MIT
/*
 * rle_codec — run-length encoding / decoding
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: stateless
 *
 * Simple RLE codec: encodes byte sequences as [count][value] pairs.
 * Count is uint8_t (1..255). Runs longer than 255 are split into
 * multiple pairs. Useful for compressing sensor data, bitmaps, or
 * any data with long runs of repeated bytes.
 *
 * Pure functions — no internal state, no heap.
 */

#ifndef RLE_CODEC_H
#define RLE_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RLE_CODEC_OK           =  0,
    RLE_CODEC_ERR_PARAM    = -1,
    RLE_CODEC_ERR_OVERFLOW = -2,
} rle_codec_err_t;

/** Encode src into dst as [count][value] pairs.
 *  @param src      input data
 *  @param src_len  length of input data
 *  @param dst      output buffer for encoded data
 *  @param dst_cap  capacity of output buffer
 *  @param out_len  [out] actual encoded length written to dst
 *  @return RLE_CODEC_OK on success
 *          RLE_CODEC_ERR_PARAM if any pointer is NULL
 *          RLE_CODEC_ERR_OVERFLOW if dst_cap is insufficient
 *  @thread_safe: yes (pure function) @isr_safe: yes */
rle_codec_err_t rle_codec_encode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len);

/** Decode [count][value] pairs from src into dst.
 *  @param src      encoded input data
 *  @param src_len  length of encoded data (must be even)
 *  @param dst      output buffer for decoded data
 *  @param dst_cap  capacity of output buffer
 *  @param out_len  [out] actual decoded length written to dst
 *  @return RLE_CODEC_OK on success
 *          RLE_CODEC_ERR_PARAM if any pointer is NULL or src_len is odd
 *          RLE_CODEC_ERR_OVERFLOW if dst_cap is insufficient
 *  @thread_safe: yes (pure function) @isr_safe: yes */
rle_codec_err_t rle_codec_decode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len);

/** Compute worst-case encoded size for a given input length.
 *  Worst case: every byte differs → 2 * src_len.
 *  @thread_safe: yes @isr_safe: yes */
size_t rle_codec_encode_bound(size_t src_len);

/** Compute worst-case decoded size for a given encoded length.
 *  Worst case: all pairs are [255][val] → (src_len / 2) * 255.
 *  Returns 0 if src_len is odd (invalid).
 *  @thread_safe: yes @isr_safe: yes */
size_t rle_codec_decode_bound(size_t src_len);

#ifdef __cplusplus
}
#endif

#endif /* RLE_CODEC_H */
