// SPDX-License-Identifier: MIT
/*
 * bitstream — bit-level read/write over a byte buffer
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Reads/writes N-bit unsigned values from/to a byte buffer at arbitrary
 * bit offsets. Useful for packed protocols (CRSF, SBUS, CAN signals).
 * LSB-first packing. Pure C99, no heap, no OS.
 */

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BITSTREAM_OK           =  0,
    BITSTREAM_ERR_PARAM    = -1,
    BITSTREAM_ERR_OVERFLOW = -2,
} bitstream_err_t;

typedef struct {
    uint8_t *buf;       /* internal: backing buffer */
    size_t   cap_bits;  /* internal: buffer capacity in bits */
    size_t   pos;       /* internal: current bit position */
} bitstream_t;

/** Init reader/writer over a buffer. @thread_safe: no @isr_safe: yes */
bitstream_err_t bitstream_init(bitstream_t *ctx, uint8_t *buf, size_t cap_bytes);

/** Write up to 32 bits (LSB first). @thread_safe: no @isr_safe: yes */
bitstream_err_t bitstream_write(bitstream_t *ctx, uint32_t value, uint8_t nbits);

/** Read up to 32 bits (LSB first). @thread_safe: no @isr_safe: yes */
bitstream_err_t bitstream_read(bitstream_t *ctx, uint8_t nbits, uint32_t *out);

/** Return current bit position. @thread_safe: no @isr_safe: yes */
size_t bitstream_tell(const bitstream_t *ctx);

/** Seek to a bit position. @thread_safe: no @isr_safe: yes */
bitstream_err_t bitstream_seek(bitstream_t *ctx, size_t bit_pos);

/** Reset position to 0. @thread_safe: no @isr_safe: yes */
bitstream_err_t bitstream_reset(bitstream_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* BITSTREAM_H */
