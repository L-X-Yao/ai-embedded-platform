// SPDX-License-Identifier: MIT
/*
 * prop_base64.c — property-based tests for base64
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_base64.c ../base64.c -o prop_base64
 */

#include "test_harness.h"
#include "property_test.h"
#include "../base64.h"
#include <string.h>

/* Property: encode then decode returns original data */
PROPERTY(encode_decode_roundtrip, 10000) {
    uint8_t input[192];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 192);
    ARBI_FILL(seed, input, input_len);

    char encoded[300];
    uint16_t enc_len = sizeof(encoded);
    base64_err_t err = base64_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, BASE64_OK);

    uint8_t decoded[192];
    uint16_t dec_len = sizeof(decoded);
    err = base64_decode(encoded, enc_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, BASE64_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: encoded length is always ceil(input_len / 3) * 4 */
PROPERTY(encoded_length_formula, 5000) {
    uint8_t input[100];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 100);
    ARBI_FILL(seed, input, input_len);

    char encoded[200];
    uint16_t enc_len = sizeof(encoded);
    base64_encode(input, input_len, encoded, &enc_len);

    uint16_t expected = (uint16_t)(((input_len + 2) / 3) * 4);
    PROP_ASSERT_EQ(enc_len, expected);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    char garbage[256];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 256);
    ARBI_FILL(seed, (uint8_t *)garbage, len);

    uint8_t out[256];
    uint16_t out_len = sizeof(out);
    base64_decode(garbage, len, out, &out_len);
}

/* Property: encode output contains only valid base64 chars */
PROPERTY(encode_valid_alphabet, 5000) {
    uint8_t input[64];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, input, input_len);

    char encoded[100];
    uint16_t enc_len = sizeof(encoded);
    base64_encode(input, input_len, encoded, &enc_len);

    for (uint16_t i = 0; i < enc_len; i++) {
        char c = encoded[i];
        int valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
        PROP_ASSERT(valid);
    }
}

TEST_MAIN("base64 properties") {
    RUN(encode_decode_roundtrip);
    RUN(encoded_length_formula);
    RUN(decode_no_crash);
    RUN(encode_valid_alphabet);
}
TEST_MAIN_END()
