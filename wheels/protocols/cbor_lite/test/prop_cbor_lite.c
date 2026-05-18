// SPDX-License-Identifier: MIT
/*
 * prop_cbor_lite.c — property-based tests for cbor_lite
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_cbor_lite.c ../cbor_lite.c -o prop_cbor_lite
 */

#include "test_harness.h"
#include "property_test.h"
#include "../cbor_lite.h"
#include <string.h>

/* Property: uint encode-decode roundtrip */
PROPERTY(uint_roundtrip, 10000) {
    uint64_t val = ((uint64_t)ARBI_UINT32(seed) << 32) | ARBI_UINT32(seed);

    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    cbor_lite_err_t err = cbor_lite_enc_uint(&enc, val);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);

    size_t enc_len = cbor_lite_enc_len(&enc);
    PROP_ASSERT(enc_len > 0);

    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, buf, enc_len);
    cbor_lite_item_t item;
    err = cbor_lite_dec_next(&dec, &item);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);
    PROP_ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
    PROP_ASSERT(item.uval == val);
}

/* Property: negative int encode-decode roundtrip */
PROPERTY(nint_roundtrip, 10000) {
    /* nint encodes -1 - val, so val=0 means -1, val=99 means -100 */
    uint64_t val = (uint64_t)ARBI_UINT32(seed);

    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    cbor_lite_err_t err = cbor_lite_enc_nint(&enc, val);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);

    size_t enc_len = cbor_lite_enc_len(&enc);
    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, buf, enc_len);
    cbor_lite_item_t item;
    err = cbor_lite_dec_next(&dec, &item);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);
    PROP_ASSERT_EQ(item.major, CBOR_MAJOR_NINT);
    PROP_ASSERT(item.uval == val);
}

/* Property: text string encode-decode roundtrip */
PROPERTY(tstr_roundtrip, 10000) {
    char text[128];
    uint8_t tlen = (uint8_t)ARBI_RANGE(seed, 0, 127);
    for (uint8_t i = 0; i < tlen; i++)
        text[i] = (char)ARBI_RANGE(seed, 0x20, 0x7E);
    text[tlen] = '\0';

    uint8_t buf[256];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    cbor_lite_err_t err = cbor_lite_enc_tstr(&enc, text, tlen);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);

    size_t enc_len = cbor_lite_enc_len(&enc);
    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, buf, enc_len);
    cbor_lite_item_t item;
    err = cbor_lite_dec_next(&dec, &item);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);
    PROP_ASSERT_EQ(item.major, CBOR_MAJOR_TSTR);
    PROP_ASSERT_EQ(item.str_len, tlen);
    if (tlen > 0) {
        PROP_ASSERT_MEM_EQ(item.str, text, tlen);
    }
}

/* Property: array header + uint items roundtrip */
PROPERTY(array_roundtrip, 5000) {
    uint8_t count = (uint8_t)ARBI_RANGE(seed, 0, 10);
    uint32_t values[10];
    for (uint8_t i = 0; i < count; i++)
        values[i] = ARBI_UINT32(seed);

    uint8_t buf[256];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    cbor_lite_enc_array(&enc, count);
    for (uint8_t i = 0; i < count; i++)
        cbor_lite_enc_uint(&enc, values[i]);

    size_t enc_len = cbor_lite_enc_len(&enc);
    PROP_ASSERT(enc_len > 0);

    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, buf, enc_len);
    cbor_lite_item_t item;

    cbor_lite_err_t err = cbor_lite_dec_next(&dec, &item);
    PROP_ASSERT_EQ(err, CBOR_LITE_OK);
    PROP_ASSERT_EQ(item.major, CBOR_MAJOR_ARRAY);
    PROP_ASSERT_EQ(item.uval, count);

    for (uint8_t i = 0; i < count; i++) {
        err = cbor_lite_dec_next(&dec, &item);
        PROP_ASSERT_EQ(err, CBOR_LITE_OK);
        PROP_ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
        PROP_ASSERT(item.uval == (uint64_t)values[i]);
    }
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[256];
    size_t len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, garbage, len);

    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, garbage, len);
    cbor_lite_item_t item;

    /* Consume up to 32 items or until error/end */
    for (int i = 0; i < 32; i++) {
        cbor_lite_err_t err = cbor_lite_dec_next(&dec, &item);
        if (err != CBOR_LITE_OK)
            break;
    }
}

TEST_MAIN("cbor_lite properties") {
    RUN(uint_roundtrip);
    RUN(nint_roundtrip);
    RUN(tstr_roundtrip);
    RUN(array_roundtrip);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
