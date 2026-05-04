/*
 * prop_msgpack.c — property-based tests for msgpack
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_msgpack.c ../msgpack.c -o prop_msgpack
 */

#include "test_harness.h"
#include "property_test.h"
#include "../msgpack.h"
#include <string.h>

/* Property: unsigned int encode-decode roundtrip */
PROPERTY(uint_roundtrip, 10000) {
    uint32_t val = ARBI_UINT32(seed);

    uint8_t buf[16];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, buf, sizeof(buf));
    msgpack_err_t err = msgpack_enc_uint(&enc, val);
    PROP_ASSERT_EQ(err, MSGPACK_OK);

    size_t enc_len = msgpack_enc_len(&enc);
    PROP_ASSERT(enc_len > 0);

    msgpack_dec_t dec;
    msgpack_dec_init(&dec, buf, enc_len);
    msgpack_token_t tok;
    err = msgpack_dec_next(&dec, &tok);
    PROP_ASSERT_EQ(err, MSGPACK_OK);
    PROP_ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
    PROP_ASSERT_EQ(tok.val.u32, val);
}

/* Property: signed int encode-decode roundtrip */
PROPERTY(int_roundtrip, 10000) {
    int32_t val = (int32_t)ARBI_UINT32(seed);

    uint8_t buf[16];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, buf, sizeof(buf));
    msgpack_err_t err = msgpack_enc_int(&enc, val);
    PROP_ASSERT_EQ(err, MSGPACK_OK);

    size_t enc_len = msgpack_enc_len(&enc);
    msgpack_dec_t dec;
    msgpack_dec_init(&dec, buf, enc_len);
    msgpack_token_t tok;
    err = msgpack_dec_next(&dec, &tok);
    PROP_ASSERT_EQ(err, MSGPACK_OK);

    /* Positive values may decode as UINT, negative as INT */
    if (val >= 0) {
        PROP_ASSERT(tok.type == MSGPACK_TYPE_UINT || tok.type == MSGPACK_TYPE_INT);
        if (tok.type == MSGPACK_TYPE_UINT)
            PROP_ASSERT_EQ(tok.val.u32, (uint32_t)val);
        else
            PROP_ASSERT_EQ(tok.val.i32, val);
    } else {
        PROP_ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
        PROP_ASSERT_EQ(tok.val.i32, val);
    }
}

/* Property: string encode-decode roundtrip */
PROPERTY(str_roundtrip, 10000) {
    char text[128];
    uint8_t tlen = (uint8_t)ARBI_RANGE(seed, 0, 127);
    for (uint8_t i = 0; i < tlen; i++)
        text[i] = (char)ARBI_RANGE(seed, 0x20, 0x7E);

    uint8_t buf[256];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, buf, sizeof(buf));
    msgpack_err_t err = msgpack_enc_str(&enc, text, tlen);
    PROP_ASSERT_EQ(err, MSGPACK_OK);

    size_t enc_len = msgpack_enc_len(&enc);
    msgpack_dec_t dec;
    msgpack_dec_init(&dec, buf, enc_len);
    msgpack_token_t tok;
    err = msgpack_dec_next(&dec, &tok);
    PROP_ASSERT_EQ(err, MSGPACK_OK);
    PROP_ASSERT_EQ(tok.type, MSGPACK_TYPE_STR);
    PROP_ASSERT_EQ(tok.val.str.len, tlen);
    if (tlen > 0) {
        PROP_ASSERT_MEM_EQ(tok.val.str.ptr, text, tlen);
    }
}

/* Property: encode is deterministic */
PROPERTY(encode_determinism, 5000) {
    uint32_t uval = ARBI_UINT32(seed);
    int32_t ival = (int32_t)ARBI_UINT32(seed);
    char s[16];
    uint8_t slen = (uint8_t)ARBI_RANGE(seed, 1, 15);
    ARBI_FILL(seed, (uint8_t *)s, slen);

    uint8_t buf1[128], buf2[128];
    msgpack_enc_t e1, e2;
    msgpack_enc_init(&e1, buf1, sizeof(buf1));
    msgpack_enc_init(&e2, buf2, sizeof(buf2));

    msgpack_enc_uint(&e1, uval);
    msgpack_enc_int(&e1, ival);
    msgpack_enc_str(&e1, s, slen);
    msgpack_enc_nil(&e1);
    msgpack_enc_bool(&e1, true);

    msgpack_enc_uint(&e2, uval);
    msgpack_enc_int(&e2, ival);
    msgpack_enc_str(&e2, s, slen);
    msgpack_enc_nil(&e2);
    msgpack_enc_bool(&e2, true);

    size_t len1 = msgpack_enc_len(&e1);
    size_t len2 = msgpack_enc_len(&e2);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, len1);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[256];
    size_t len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, garbage, len);

    msgpack_dec_t dec;
    msgpack_dec_init(&dec, garbage, len);
    msgpack_token_t tok;

    for (int i = 0; i < 32; i++) {
        msgpack_err_t err = msgpack_dec_next(&dec, &tok);
        if (err != MSGPACK_OK)
            break;
    }
}

TEST_MAIN("msgpack properties") {
    RUN(uint_roundtrip);
    RUN(int_roundtrip);
    RUN(str_roundtrip);
    RUN(encode_determinism);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
