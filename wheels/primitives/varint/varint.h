// SPDX-License-Identifier: MIT
/*
 * varint — variable-length integer encoding (LEB128 / protobuf)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * LEB128: 7 data bits per byte, bit 7 = continuation flag.
 * Signed uses ZigZag: (n << 1) ^ (n >> 63).
 * Pure C99, no heap, no OS.
 */

#ifndef VARINT_H
#define VARINT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VARINT_OK           =  0,
    VARINT_ERR_PARAM    = -1,
    VARINT_ERR_OVERFLOW = -2,
    VARINT_ERR_SHORT    = -3,
} varint_err_t;

/** Encode uint64. @thread_safe: yes @isr_safe: yes */
varint_err_t varint_encode_u64(uint64_t value, uint8_t *buf, size_t cap,
                                size_t *out_len);

/** Decode uint64.  VARINT_ERR_OVERFLOW if the value needs more than 64 bits,
 *  VARINT_ERR_SHORT if buf ends first. @thread_safe: yes @isr_safe: yes */
varint_err_t varint_decode_u64(const uint8_t *buf, size_t len,
                                uint64_t *value, size_t *out_len);

/** Encode int64 (ZigZag). @thread_safe: yes @isr_safe: yes */
varint_err_t varint_encode_s64(int64_t value, uint8_t *buf, size_t cap,
                                size_t *out_len);

/** Decode int64 (ZigZag). @thread_safe: yes @isr_safe: yes */
varint_err_t varint_decode_s64(const uint8_t *buf, size_t len,
                                int64_t *value, size_t *out_len);

/** Encoded size of uint64 (1-10). @thread_safe: yes @isr_safe: yes */
size_t varint_encoded_size_u64(uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* VARINT_H */
