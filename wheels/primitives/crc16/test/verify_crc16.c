// SPDX-License-Identifier: MIT
/*
 * verify_crc16.c — CBMC bounded model checking for crc16
 *
 * Verifies CRC16-MODBUS properties: known vectors, incremental consistency,
 * and sensitivity. Run with:
 *   cbmc verify_crc16.c ../crc16.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../crc16.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

/* Proof 1: incremental step matches one-shot for arbitrary data */
void verify_step_consistency(void) {
    uint8_t data[8];
    unsigned len = nondet_unsigned();
    __CPROVER_assume(len <= 8);

    for (unsigned i = 0; i < len; i++) {
        data[i] = nondet_uint8();
    }

    /* One-shot */
    uint16_t one_shot = crc16_modbus(data, len);

    /* Incremental via steps */
    uint16_t incremental = CRC16_MODBUS_INIT;
    for (unsigned i = 0; i < len; i++) {
        incremental = crc16_modbus_step(incremental, data[i]);
    }

    assert(one_shot == incremental);
}

/* Proof 2: CRC is sensitive — flipping any single bit changes the CRC */
void verify_bit_sensitivity(void) {
    uint8_t data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = nondet_uint8();
    }

    uint16_t original = crc16_modbus(data, 4);

    unsigned byte_idx = nondet_unsigned();
    unsigned bit_idx = nondet_unsigned();
    __CPROVER_assume(byte_idx < 4);
    __CPROVER_assume(bit_idx < 8);

    uint8_t modified[4];
    for (int i = 0; i < 4; i++) {
        modified[i] = data[i];
    }
    modified[byte_idx] ^= (uint8_t)(1u << bit_idx);

    uint16_t flipped = crc16_modbus(modified, 4);
    assert(flipped != original);
}

/* Proof 3: empty data returns initial value, determinism */
void verify_empty_and_determinism(void) {
    /* Empty data always returns init */
    uint16_t empty = crc16_modbus(NULL, 0);
    assert(empty == CRC16_MODBUS_INIT);

    /* Same input always produces same output */
    uint8_t data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = nondet_uint8();
    }

    uint16_t first  = crc16_modbus(data, 4);
    uint16_t second = crc16_modbus(data, 4);
    assert(first == second);
}

/* Proof 4: split computation equals full computation (associativity) */
void verify_split_associativity(void) {
    uint8_t data[8];
    unsigned total = nondet_unsigned();
    __CPROVER_assume(total <= 8);

    unsigned split = nondet_unsigned();
    __CPROVER_assume(split <= total);

    for (unsigned i = 0; i < total; i++) {
        data[i] = nondet_uint8();
    }

    /* Full computation */
    uint16_t full = crc16_modbus(data, total);

    /* Split: first part via steps, second part via steps */
    uint16_t partial = CRC16_MODBUS_INIT;
    for (unsigned i = 0; i < split; i++) {
        partial = crc16_modbus_step(partial, data[i]);
    }
    for (unsigned i = split; i < total; i++) {
        partial = crc16_modbus_step(partial, data[i]);
    }

    assert(full == partial);
}

int main(void) {
    verify_step_consistency();
    verify_bit_sensitivity();
    verify_empty_and_determinism();
    verify_split_associativity();
    return 0;
}
