/*
 * prop_cobs.c — property-based tests for cobs
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_cobs.c ../cobs.c -o prop_cobs
 */

#include "test_harness.h"
#include "property_test.h"
#include "../cobs.h"
#include <string.h>

/* Property: encode then decode returns original data */
PROPERTY(encode_decode_roundtrip, 10000) {
    uint8_t input[200];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 200);
    ARBI_FILL(seed, input, input_len);

    uint16_t max_enc = cobs_max_encoded_size(input_len);
    uint8_t encoded[512];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, COBS_OK);
    PROP_ASSERT(enc_len <= max_enc);

    uint8_t decoded[200];
    uint16_t dec_len = (uint16_t)sizeof(decoded);
    err = cobs_decode(encoded, enc_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, COBS_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: encoded output contains no 0x00 bytes */
PROPERTY(no_zero_in_encoded, 10000) {
    uint8_t input[128];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 1, 128);
    ARBI_FILL(seed, input, input_len);

    uint8_t encoded[384];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, COBS_OK);

    for (uint16_t i = 0; i < enc_len; i++) {
        PROP_ASSERT(encoded[i] != 0x00);
    }
}

/* Property: decode of arbitrary non-zero bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    uint8_t garbage[256];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 256);
    ARBI_FILL(seed, garbage, len);
    /* ensure no zero bytes to avoid trivially invalid frames */
    for (uint16_t i = 0; i < len; i++) {
        if (garbage[i] == 0) garbage[i] = 1;
    }

    uint8_t out[256];
    uint16_t out_len = (uint16_t)sizeof(out);
    cobs_decode(garbage, len, out, &out_len);
    /* no crash is the assertion */
}

/* Property: encoding empty data roundtrips */
PROPERTY(empty_roundtrip, 1000) {
    uint8_t encoded[16];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(NULL, 0, encoded, &enc_len);
    PROP_ASSERT_EQ(err, COBS_OK);

    uint8_t decoded[16];
    uint16_t dec_len = (uint16_t)sizeof(decoded);
    err = cobs_decode(encoded, enc_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, COBS_OK);
    PROP_ASSERT_EQ(dec_len, 0);
}

/* Property: encoded size is bounded by max_encoded_size */
PROPERTY(encoded_size_bounded, 10000) {
    uint8_t input[200];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 200);
    ARBI_FILL(seed, input, input_len);

    uint16_t max_enc = cobs_max_encoded_size(input_len);

    uint8_t encoded[512];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    cobs_err_t err = cobs_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, COBS_OK);
    PROP_ASSERT(enc_len <= max_enc);
}

TEST_MAIN("cobs properties") {
    RUN(encode_decode_roundtrip);
    RUN(no_zero_in_encoded);
    RUN(decode_no_crash);
    RUN(empty_roundtrip);
    RUN(encoded_size_bounded);
}
TEST_MAIN_END()
