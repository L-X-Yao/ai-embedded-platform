// SPDX-License-Identifier: MIT
/*
 * bitfield — bit-level pack/unpack from byte arrays
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Big-endian bit numbering (MSB-first): bit 0 is the MSB of byte 0.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef BITFIELD_H
#define BITFIELD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @thread_safe: yes (pure function, no state) */
int bitfield_get(const uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count,
                 uint32_t *out);

/** Pack value into bit range in byte array. @thread_safe: yes */
int bitfield_set(uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count,
                 uint32_t value);

/** Return true if bit at given index is set. @thread_safe: yes */
bool bitfield_test_bit(const uint8_t *data, size_t data_len, size_t bit_index);

/** Set a single bit to 1. @thread_safe: yes */
void bitfield_set_bit(uint8_t *data, size_t data_len, size_t bit_index);

/** Clear a single bit to 0. @thread_safe: yes */
void bitfield_clear_bit(uint8_t *data, size_t data_len, size_t bit_index);

#define BITFIELD_OK           0
#define BITFIELD_ERR_NULL    -1
#define BITFIELD_ERR_RANGE   -2
#define BITFIELD_ERR_WIDTH   -3

#ifdef __cplusplus
}
#endif

#endif /* BITFIELD_H */
