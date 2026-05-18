// SPDX-License-Identifier: MIT
/*
 * prop_bencode.c — property-based tests for bencode
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_bencode.c ../bencode.c -o prop_bencode
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bencode.h"
#include <string.h>

/* Property: encode integer then decode roundtrip */
PROPERTY(int_roundtrip, 20000) {
    int64_t value = (int64_t)(int32_t)ARBI_UINT32(seed);

    uint8_t buf[64];
    bencode_encoder_t enc;
    bencode_encoder_init(&enc, buf, sizeof(buf));
    bencode_err_t err = bencode_encode_int(&enc, value);
    PROP_ASSERT_EQ(err, BENCODE_OK);

    bencode_decoder_t dec;
    bencode_decoder_init(&dec, buf, bencode_encoder_len(&enc));
    bencode_token_t tok;
    memset(&tok, 0, sizeof(tok));
    err = bencode_decode_next(&dec, &tok);
    PROP_ASSERT_EQ(err, BENCODE_OK);
    PROP_ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    PROP_ASSERT_EQ(tok.value.integer, value);
}

/* Property: encode string then decode roundtrip */
PROPERTY(string_roundtrip, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, data, len);

    uint8_t buf[128];
    bencode_encoder_t enc;
    bencode_encoder_init(&enc, buf, sizeof(buf));
    bencode_err_t err = bencode_encode_string(&enc, data, len);
    PROP_ASSERT_EQ(err, BENCODE_OK);

    bencode_decoder_t dec;
    bencode_decoder_init(&dec, buf, bencode_encoder_len(&enc));
    bencode_token_t tok;
    memset(&tok, 0, sizeof(tok));
    err = bencode_decode_next(&dec, &tok);
    PROP_ASSERT_EQ(err, BENCODE_OK);
    PROP_ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    PROP_ASSERT_EQ(tok.value.string.len, len);
    if (len > 0) {
        PROP_ASSERT_MEM_EQ(tok.value.string.data, data, len);
    }
}

/* Property: encode is deterministic */
PROPERTY(encode_determinism, 5000) {
    int64_t value = (int64_t)(int32_t)ARBI_UINT32(seed);

    uint8_t buf1[64], buf2[64];
    bencode_encoder_t enc1, enc2;
    bencode_encoder_init(&enc1, buf1, sizeof(buf1));
    bencode_encoder_init(&enc2, buf2, sizeof(buf2));
    bencode_encode_int(&enc1, value);
    bencode_encode_int(&enc2, value);
    size_t len1 = bencode_encoder_len(&enc1);
    size_t len2 = bencode_encoder_len(&enc2);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, len1);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, garbage, len);

    bencode_decoder_t dec;
    bencode_decoder_init(&dec, garbage, len);
    bencode_token_t tok;
    for (int i = 0; i < 20; i++) {
        memset(&tok, 0, sizeof(tok));
        bencode_err_t err = bencode_decode_next(&dec, &tok);
        if (err != BENCODE_OK)
            break;
    }
}

TEST_MAIN("bencode properties") {
    RUN(int_roundtrip);
    RUN(string_roundtrip);
    RUN(encode_determinism);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
