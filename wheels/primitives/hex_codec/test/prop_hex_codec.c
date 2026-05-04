/*
 * prop_hex_codec.c — property-based tests for hex_codec
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_hex_codec.c ../hex_codec.c -o prop_hex_codec
 */

#include "test_harness.h"
#include "property_test.h"
#include "../hex_codec.h"
#include <string.h>

/* Property: encode then decode returns original data */
PROPERTY(encode_decode_roundtrip, 10000) {
    uint8_t input[128];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, input, input_len);

    char encoded[257]; /* 128*2 + 1 */
    hex_err_t err = hex_encode(input, input_len, encoded, sizeof(encoded));
    PROP_ASSERT_EQ(err, HEX_OK);

    uint8_t decoded[128];
    uint16_t dec_len = sizeof(decoded);
    uint16_t enc_str_len = hex_encoded_size(input_len);
    err = hex_decode(encoded, enc_str_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, HEX_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: upper-case encode then decode also roundtrips */
PROPERTY(encode_upper_roundtrip, 5000) {
    uint8_t input[64];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 64);
    ARBI_FILL(seed, input, input_len);

    char encoded[129]; /* 64*2 + 1 */
    hex_err_t err = hex_encode_upper(input, input_len, encoded, sizeof(encoded));
    PROP_ASSERT_EQ(err, HEX_OK);

    uint8_t decoded[64];
    uint16_t dec_len = sizeof(decoded);
    uint16_t enc_str_len = hex_encoded_size(input_len);
    err = hex_decode(encoded, enc_str_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, HEX_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: encoded length is always data_len * 2 */
PROPERTY(encoded_length_formula, 5000) {
    uint8_t input[100];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 100);
    ARBI_FILL(seed, input, input_len);

    uint16_t expected = (uint16_t)(input_len * 2);
    PROP_ASSERT_EQ(hex_encoded_size(input_len), expected);
    PROP_ASSERT_EQ(hex_decoded_size(expected), input_len);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    char garbage[256];
    uint16_t len = (uint16_t)(ARBI_RANGE(seed, 0, 128) * 2); /* even length */
    ARBI_FILL(seed, (uint8_t *)garbage, len);

    uint8_t out[128];
    uint16_t out_len = sizeof(out);
    hex_decode(garbage, len, out, &out_len);
    /* no crash is the assertion */
}

/* Property: encode output contains only valid hex chars */
PROPERTY(encode_valid_alphabet, 5000) {
    uint8_t input[32];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 1, 32);
    ARBI_FILL(seed, input, input_len);

    char encoded[65]; /* 32*2 + 1 */
    hex_encode(input, input_len, encoded, sizeof(encoded));

    uint16_t enc_len = hex_encoded_size(input_len);
    for (uint16_t i = 0; i < enc_len; i++) {
        char c = encoded[i];
        int valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        PROP_ASSERT(valid);
    }
}

TEST_MAIN("hex_codec properties") {
    RUN(encode_decode_roundtrip);
    RUN(encode_upper_roundtrip);
    RUN(encoded_length_formula);
    RUN(decode_no_crash);
    RUN(encode_valid_alphabet);
}
TEST_MAIN_END()
