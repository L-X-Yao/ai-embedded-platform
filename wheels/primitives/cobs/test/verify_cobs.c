// SPDX-License-Identifier: MIT
/*
 * verify_cobs.c — CBMC bounded model checking for cobs
 *
 * Verifies encode-decode roundtrip, zero elimination, and size bounds.
 * Run with:
 *   cbmc verify_cobs.c ../cobs.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../cobs.h"
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define MAX_LEN 16

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

/* Proof 1: encode-then-decode roundtrip preserves data */
void verify_roundtrip(void) {
    uint8_t input[MAX_LEN];
    unsigned len = nondet_unsigned();
    __CPROVER_assume(len <= MAX_LEN);

    for (unsigned i = 0; i < len; i++) {
        input[i] = nondet_uint8();
    }

    uint16_t enc_cap = cobs_max_encoded_size((uint16_t)len);
    uint8_t encoded[MAX_LEN + 2];  /* worst case for 16 bytes */
    __CPROVER_assume(enc_cap <= sizeof(encoded));

    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(input, (uint16_t)len, encoded, &enc_len);
    assert(err == COBS_OK);

    uint8_t decoded[MAX_LEN];
    uint16_t dec_len = (uint16_t)sizeof(decoded);
    err = cobs_decode(encoded, enc_len, decoded, &dec_len);
    assert(err == COBS_OK);
    assert(dec_len == (uint16_t)len);

    for (unsigned i = 0; i < len; i++) {
        assert(decoded[i] == input[i]);
    }
}

/* Proof 2: encoded output contains no zero bytes */
void verify_no_zeros_in_encoded(void) {
    uint8_t input[8];
    unsigned len = nondet_unsigned();
    __CPROVER_assume(len >= 1 && len <= 8);

    for (unsigned i = 0; i < len; i++) {
        input[i] = nondet_uint8();
    }

    uint8_t encoded[12];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(input, (uint16_t)len, encoded, &enc_len);
    assert(err == COBS_OK);

    for (uint16_t i = 0; i < enc_len; i++) {
        assert(encoded[i] != 0x00);
    }
}

/* Proof 3: encoded size is within documented bounds */
void verify_size_bounds(void) {
    uint16_t len = nondet_uint16();
    __CPROVER_assume(len <= 254);

    uint16_t max_enc = cobs_max_encoded_size(len);

    /* COBS overhead: at most ceil(len/254) + 1 */
    /* For len <= 254, max is len + 1 */
    assert(max_enc >= (uint16_t)(len + 1));

    /* Actual encoded size must not exceed max */
    if (len > 0 && len <= 8) {
        uint8_t input[8];
        for (uint16_t i = 0; i < len; i++) {
            input[i] = nondet_uint8();
        }
        uint8_t encoded[12];
        uint16_t enc_len = (uint16_t)sizeof(encoded);
        cobs_err_t err = cobs_encode(input, len, encoded, &enc_len);
        assert(err == COBS_OK);
        assert(enc_len <= max_enc);
    }
}

/* Proof 4: NULL and overflow safety */
void verify_null_safety(void) {
    cobs_err_t err;

    /* NULL input with zero length should succeed */
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    err = cobs_encode(NULL, 0, out, &out_len);
    assert(err == COBS_OK);

    /* NULL output pointer */
    uint8_t data[4] = {1, 2, 3, 4};
    err = cobs_encode(data, 4, NULL, &out_len);
    assert(err == COBS_ERR_PARAM);

    /* NULL out_len pointer */
    err = cobs_encode(data, 4, out, NULL);
    assert(err == COBS_ERR_PARAM);

    /* Output buffer too small */
    uint16_t tiny = 1;
    err = cobs_encode(data, 4, out, &tiny);
    assert(err == COBS_ERR_OVERFLOW);
}

int main(void) {
    verify_roundtrip();
    verify_no_zeros_in_encoded();
    verify_size_bounds();
    verify_null_safety();
    return 0;
}
