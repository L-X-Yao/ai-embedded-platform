/*
 * prop_tlv_codec.c — property-based tests for tlv_codec
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_tlv_codec.c ../tlv_codec.c -o prop_tlv_codec
 */

#include "test_harness.h"
#include "property_test.h"
#include "../tlv_codec.h"
#include <string.h>

/* Property: raw TLV encode-decode roundtrip */
PROPERTY(raw_roundtrip, 10000) {
    uint16_t tag = ARBI_UINT16(seed);
    uint8_t value[128];
    uint16_t vlen = (uint16_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, value, vlen);

    uint8_t buf[512];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    tlv_err_t err = tlv_encode(&enc, tag, value, vlen);
    PROP_ASSERT_EQ(err, TLV_OK);

    uint16_t enc_len = tlv_encoder_len(&enc);
    PROP_ASSERT(enc_len > 0);
    /* Expected: 2 (tag) + 2 (length) + vlen */
    PROP_ASSERT_EQ(enc_len, 4 + vlen);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, enc_len);
    tlv_t tlv;
    err = tlv_decode_next(&dec, &tlv);
    PROP_ASSERT_EQ(err, TLV_OK);
    PROP_ASSERT_EQ(tlv.tag, tag);
    PROP_ASSERT_EQ(tlv.length, vlen);
    if (vlen > 0) {
        PROP_ASSERT_MEM_EQ(tlv.value, value, vlen);
    }
    PROP_ASSERT(tlv_decoder_done(&dec));
}

/* Property: typed encode helpers roundtrip */
PROPERTY(typed_roundtrip, 10000) {
    uint16_t tag = ARBI_UINT16(seed);
    uint8_t u8val = ARBI_UINT8(seed);
    uint16_t u16val = ARBI_UINT16(seed);
    uint32_t u32val = ARBI_UINT32(seed);

    uint8_t buf[256];
    tlv_encoder_t enc;

    /* u8 */
    tlv_encoder_init(&enc, buf, sizeof(buf));
    tlv_err_t err = tlv_encode_u8(&enc, tag, u8val);
    PROP_ASSERT_EQ(err, TLV_OK);
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    tlv_t tlv;
    err = tlv_decode_next(&dec, &tlv);
    PROP_ASSERT_EQ(err, TLV_OK);
    PROP_ASSERT_EQ(tlv_get_u8(&tlv), u8val);

    /* u16 */
    tlv_encoder_init(&enc, buf, sizeof(buf));
    err = tlv_encode_u16(&enc, tag, u16val);
    PROP_ASSERT_EQ(err, TLV_OK);
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    err = tlv_decode_next(&dec, &tlv);
    PROP_ASSERT_EQ(err, TLV_OK);
    PROP_ASSERT_EQ(tlv_get_u16(&tlv), u16val);

    /* u32 */
    tlv_encoder_init(&enc, buf, sizeof(buf));
    err = tlv_encode_u32(&enc, tag, u32val);
    PROP_ASSERT_EQ(err, TLV_OK);
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    err = tlv_decode_next(&dec, &tlv);
    PROP_ASSERT_EQ(err, TLV_OK);
    PROP_ASSERT_EQ(tlv_get_u32(&tlv), u32val);
}

/* Property: multiple TLVs encode then decode in order */
PROPERTY(multi_tlv_roundtrip, 5000) {
    uint8_t count = (uint8_t)ARBI_RANGE(seed, 1, 8);
    uint16_t tags[8];
    uint8_t values[8][32];
    uint16_t vlens[8];
    for (uint8_t i = 0; i < count; i++) {
        tags[i] = ARBI_UINT16(seed);
        vlens[i] = (uint16_t)ARBI_RANGE(seed, 0, 32);
        ARBI_FILL(seed, values[i], vlens[i]);
    }

    uint8_t buf[1024];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    for (uint8_t i = 0; i < count; i++) {
        tlv_err_t err = tlv_encode(&enc, tags[i], values[i], vlens[i]);
        PROP_ASSERT_EQ(err, TLV_OK);
    }

    uint16_t total = tlv_encoder_len(&enc);
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, total);

    for (uint8_t i = 0; i < count; i++) {
        tlv_t tlv;
        tlv_err_t err = tlv_decode_next(&dec, &tlv);
        PROP_ASSERT_EQ(err, TLV_OK);
        PROP_ASSERT_EQ(tlv.tag, tags[i]);
        PROP_ASSERT_EQ(tlv.length, vlens[i]);
        if (vlens[i] > 0) {
            PROP_ASSERT_MEM_EQ(tlv.value, values[i], vlens[i]);
        }
    }
    PROP_ASSERT(tlv_decoder_done(&dec));
}

/* Property: encode is deterministic */
PROPERTY(encode_determinism, 5000) {
    uint16_t tag = ARBI_UINT16(seed);
    uint8_t val[64];
    uint16_t vlen = (uint16_t)ARBI_RANGE(seed, 0, 64);
    ARBI_FILL(seed, val, vlen);

    uint8_t buf1[256], buf2[256];
    tlv_encoder_t e1, e2;
    tlv_encoder_init(&e1, buf1, sizeof(buf1));
    tlv_encoder_init(&e2, buf2, sizeof(buf2));
    tlv_encode(&e1, tag, val, vlen);
    tlv_encode(&e2, tag, val, vlen);

    uint16_t len1 = tlv_encoder_len(&e1);
    uint16_t len2 = tlv_encoder_len(&e2);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, len1);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[256];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 256);
    ARBI_FILL(seed, garbage, len);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, garbage, len);
    tlv_t tlv;

    for (int i = 0; i < 32; i++) {
        tlv_err_t err = tlv_decode_next(&dec, &tlv);
        if (err != TLV_OK)
            break;
    }
}

TEST_MAIN("tlv_codec properties") {
    RUN(raw_roundtrip);
    RUN(typed_roundtrip);
    RUN(multi_tlv_roundtrip);
    RUN(encode_determinism);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
