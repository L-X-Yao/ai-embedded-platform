// SPDX-License-Identifier: MIT
/*
 * verify_bitfield.c — CBMC bounded model checking for bitfield
 *
 * Verifies set/get roundtrip, single-bit ops, and bounds checking.
 * Run with:
 *   cbmc verify_bitfield.c ../bitfield.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../bitfield.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);
extern uint32_t nondet_uint32(void);

/* Proof 1: set then get roundtrip preserves value */
void verify_set_get_roundtrip(void) {
    uint8_t data[8];
    memset(data, 0, sizeof(data));

    size_t bit_offset = nondet_unsigned();
    size_t bit_count  = nondet_unsigned();
    uint32_t value    = nondet_uint32();

    /* Constrain to valid ranges: offset+count fits in 64 bits, count <= 32 */
    __CPROVER_assume(bit_count >= 1 && bit_count <= 32);
    __CPROVER_assume(bit_offset <= 64 - bit_count);

    /* Mask value to bit_count bits */
    uint32_t mask = (bit_count == 32) ? 0xFFFFFFFF : ((1u << bit_count) - 1);
    uint32_t masked_val = value & mask;

    int err = bitfield_set(data, sizeof(data), bit_offset, bit_count,
                           masked_val);
    assert(err == BITFIELD_OK);

    uint32_t out = 0;
    err = bitfield_get(data, sizeof(data), bit_offset, bit_count, &out);
    assert(err == BITFIELD_OK);
    assert(out == masked_val);
}

/* Proof 2: single-bit set/test/clear consistency */
void verify_single_bit_ops(void) {
    uint8_t data[4];
    memset(data, 0, sizeof(data));

    size_t bit_idx = nondet_unsigned();
    __CPROVER_assume(bit_idx < 32);  /* 4 bytes * 8 bits */

    /* Initially clear */
    assert(bitfield_test_bit(data, sizeof(data), bit_idx) == false);

    /* Set and verify */
    bitfield_set_bit(data, sizeof(data), bit_idx);
    assert(bitfield_test_bit(data, sizeof(data), bit_idx) == true);

    /* Clear and verify */
    bitfield_clear_bit(data, sizeof(data), bit_idx);
    assert(bitfield_test_bit(data, sizeof(data), bit_idx) == false);
}

/* Proof 3: out-of-range access returns error */
void verify_bounds_checking(void) {
    uint8_t data[4];  /* 32 bits */
    uint32_t out = 0;
    int err;

    /* Offset + count exceeds buffer */
    err = bitfield_get(data, sizeof(data), 30, 8, &out);
    assert(err == BITFIELD_ERR_RANGE);

    err = bitfield_set(data, sizeof(data), 30, 8, 0);
    assert(err == BITFIELD_ERR_RANGE);

    /* bit_count == 0 should error */
    err = bitfield_get(data, sizeof(data), 0, 0, &out);
    assert(err != BITFIELD_OK);

    /* bit_count > 32 should error */
    err = bitfield_get(data, sizeof(data), 0, 33, &out);
    assert(err != BITFIELD_OK);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    uint32_t out = 0;
    int err;

    err = bitfield_get(NULL, 4, 0, 8, &out);
    assert(err == BITFIELD_ERR_NULL);

    uint8_t data[4] = {0};
    err = bitfield_get(data, 4, 0, 8, NULL);
    assert(err == BITFIELD_ERR_NULL);

    err = bitfield_set(NULL, 4, 0, 8, 0);
    assert(err == BITFIELD_ERR_NULL);

    /* test_bit on NULL returns false */
    assert(bitfield_test_bit(NULL, 4, 0) == false);
}

int main(void) {
    verify_set_get_roundtrip();
    verify_single_bit_ops();
    verify_bounds_checking();
    verify_null_safety();
    return 0;
}
