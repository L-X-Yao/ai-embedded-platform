// SPDX-License-Identifier: MIT
/*
 * prop_rle_codec.c — property-based tests for rle_codec
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_rle_codec.c ../rle_codec.c -o prop_rle_codec
 */

#include "test_harness.h"
#include "property_test.h"
#include "../rle_codec.h"
#include <string.h>

/* Property: encode then decode returns original data for arbitrary bytes */
PROPERTY(encode_decode_roundtrip, 5000) {
    uint8_t input[256];
    size_t input_len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, input, input_len);

    /* worst case: 2 bytes per input byte */
    uint8_t encoded[512];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(input, input_len, encoded, sizeof(encoded), &enc_len),
        RLE_CODEC_OK);

    uint8_t decoded[256];
    size_t dec_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_decode(encoded, enc_len, decoded, sizeof(decoded), &dec_len),
        RLE_CODEC_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: encoded length is always even (pairs of [count][value]) */
PROPERTY(encoded_length_is_even, 5000) {
    uint8_t input[256];
    size_t input_len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, input, input_len);

    uint8_t encoded[512];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(input, input_len, encoded, sizeof(encoded), &enc_len),
        RLE_CODEC_OK);
    PROP_ASSERT_EQ(enc_len % 2, 0);
}

/* Property: encoded length is bounded by rle_codec_encode_bound */
PROPERTY(encoded_length_within_bound, 5000) {
    uint8_t input[200];
    size_t input_len = ARBI_SIZE(seed, 200);
    ARBI_FILL(seed, input, input_len);

    uint8_t encoded[400];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(input, input_len, encoded, sizeof(encoded), &enc_len),
        RLE_CODEC_OK);
    PROP_ASSERT(enc_len <= rle_codec_encode_bound(input_len));
}

/* Property: encoded count fields are never zero (count == 0 is invalid format) */
PROPERTY(encoded_counts_nonzero, 5000) {
    uint8_t input[200];
    size_t input_len = (size_t)ARBI_RANGE(seed, 1, 200);
    ARBI_FILL(seed, input, input_len);

    uint8_t encoded[400];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(input, input_len, encoded, sizeof(encoded), &enc_len),
        RLE_CODEC_OK);
    for (size_t i = 0; i < enc_len; i += 2) {
        PROP_ASSERT(encoded[i] != 0);
    }
}

/* Property: idempotence — re-encoding the decoded buffer reproduces the
 * same encoded stream. */
PROPERTY(roundtrip_is_idempotent, 5000) {
    uint8_t input[128];
    size_t input_len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, input, input_len);

    uint8_t enc1[256];
    size_t enc1_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(input, input_len, enc1, sizeof(enc1), &enc1_len),
        RLE_CODEC_OK);

    uint8_t dec[128];
    size_t dec_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_decode(enc1, enc1_len, dec, sizeof(dec), &dec_len),
        RLE_CODEC_OK);

    uint8_t enc2[256];
    size_t enc2_len = 0;
    PROP_ASSERT_EQ(
        rle_codec_encode(dec, dec_len, enc2, sizeof(enc2), &enc2_len),
        RLE_CODEC_OK);
    PROP_ASSERT_EQ(enc2_len, enc1_len);
    PROP_ASSERT_MEM_EQ(enc2, enc1, enc1_len);
}

TEST_MAIN("rle_codec properties") {
    RUN(encode_decode_roundtrip);
    RUN(encoded_length_is_even);
    RUN(encoded_length_within_bound);
    RUN(encoded_counts_nonzero);
    RUN(roundtrip_is_idempotent);
}
TEST_MAIN_END()
