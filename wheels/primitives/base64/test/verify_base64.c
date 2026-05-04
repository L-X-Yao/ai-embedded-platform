/*
 * verify_base64.c — CBMC bounded model checking for base64
 *
 * Verifies encode-decode roundtrip, size calculations, and error paths.
 * Run with:
 *   cbmc verify_base64.c ../base64.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../base64.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define MAX_LEN 12

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

    uint16_t enc_size = base64_encoded_size((uint16_t)len);
    char encoded[20];  /* ceil(12/3)*4 = 16, +1 NUL, +safety */
    __CPROVER_assume(enc_size < sizeof(encoded));

    uint16_t enc_len = (uint16_t)sizeof(encoded);
    base64_err_t err = base64_encode(input, (uint16_t)len, encoded, &enc_len);
    assert(err == BASE64_OK);
    assert(enc_len == enc_size);

    uint8_t decoded[MAX_LEN];
    uint16_t dec_len = (uint16_t)sizeof(decoded);
    err = base64_decode(encoded, enc_len, decoded, &dec_len);
    assert(err == BASE64_OK);
    assert(dec_len == (uint16_t)len);

    for (unsigned i = 0; i < len; i++) {
        assert(decoded[i] == input[i]);
    }
}

/* Proof 2: encoded_size is always a multiple of 4 (RFC 4648 padding) */
void verify_size_properties(void) {
    uint16_t len = nondet_uint16();
    __CPROVER_assume(len <= 1000);

    uint16_t enc_size = base64_encoded_size(len);

    /* Base64 output is always multiple of 4 */
    if (len > 0) {
        assert((enc_size % 4) == 0);
    }

    /* Encoded size is exactly ceil(len/3)*4 */
    uint16_t expected = (uint16_t)(((len + 2) / 3) * 4);
    assert(enc_size == expected);

    /* Decoded size upper bound */
    uint16_t dec_size = base64_decoded_size(enc_size);
    assert(dec_size >= len);
}

/* Proof 3: output buffer too small returns overflow error */
void verify_overflow_detection(void) {
    uint8_t input[8];
    for (int i = 0; i < 8; i++) {
        input[i] = nondet_uint8();
    }

    /* Encode with too-small buffer */
    char small_buf[4];
    uint16_t small_len = (uint16_t)sizeof(small_buf);
    base64_err_t err = base64_encode(input, 8, small_buf, &small_len);
    assert(err == BASE64_ERR_OVERFLOW);

    /* Decode with too-small buffer */
    char valid_b64[] = "AQIDBA==";  /* encodes 4 bytes */
    uint8_t tiny[1];
    uint16_t tiny_len = 1;
    err = base64_decode(valid_b64, 8, tiny, &tiny_len);
    assert(err == BASE64_ERR_OVERFLOW);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    base64_err_t err;

    /* NULL output buffer */
    uint8_t data[4] = {1, 2, 3, 4};
    uint16_t out_len = 16;
    err = base64_encode(data, 4, NULL, &out_len);
    assert(err == BASE64_ERR_PARAM);

    /* NULL out_len */
    char buf[16];
    err = base64_encode(data, 4, buf, NULL);
    assert(err == BASE64_ERR_PARAM);

    /* NULL input with zero length should succeed */
    out_len = (uint16_t)sizeof(buf);
    err = base64_encode(NULL, 0, buf, &out_len);
    assert(err == BASE64_OK);
    assert(out_len == 0);

    /* Decode NULL input */
    uint8_t dec_buf[8];
    uint16_t dec_len = sizeof(dec_buf);
    err = base64_decode(NULL, 0, dec_buf, &dec_len);
    assert(err == BASE64_OK);
    assert(dec_len == 0);
}

int main(void) {
    verify_roundtrip();
    verify_size_properties();
    verify_overflow_detection();
    verify_null_safety();
    return 0;
}
