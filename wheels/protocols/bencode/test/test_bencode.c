// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "bencode.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Encoder tests                                                      */
/* ------------------------------------------------------------------ */

TEST(test_encode_int_positive)
{
    uint8_t buf[32];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, 42), BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 4);
    ASSERT_MEMEQ(buf, "i42e");
}

TEST(test_encode_int_zero)
{
    uint8_t buf[32];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, 0), BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 3);
    ASSERT_MEMEQ(buf, "i0e");
}

TEST(test_encode_int_negative)
{
    uint8_t buf[32];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, -7), BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 4);
    ASSERT_MEMEQ(buf, "i-7e");
}

TEST(test_encode_string)
{
    uint8_t buf[32];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"foo", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 5);
    ASSERT_MEMEQ(buf, "3:foo");
}

TEST(test_encode_empty_string)
{
    uint8_t buf[32];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, NULL, 0), BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 2);
    ASSERT_MEMEQ(buf, "0:");
}

TEST(test_encode_list)
{
    /* l3:fooi42ee */
    uint8_t buf[64];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_list_begin(&enc), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"foo", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, 42), BENCODE_OK);
    ASSERT_EQ(bencode_encode_list_end(&enc), BENCODE_OK);
    ASSERT_EQ(bencode_encoder_len(&enc), 11);
    ASSERT_TRUE(memcmp(buf, "l3:fooi42ee", 11) == 0);
}

TEST(test_encode_dict)
{
    /* d3:agei25e4:name3:bobe */
    uint8_t buf[64];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_dict_begin(&enc), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"age", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, 25), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"name", 4),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"bob", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_dict_end(&enc), BENCODE_OK);
    size_t len = bencode_encoder_len(&enc);
    ASSERT_EQ(len, 22);
    ASSERT_TRUE(memcmp(buf, "d3:agei25e4:name3:bobe", 22) == 0);
}

TEST(test_encode_overflow)
{
    uint8_t buf[3];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    /* i42e needs 4 bytes, only 3 available */
    ASSERT_EQ(bencode_encode_int(&enc, 42), BENCODE_ERR_OVERFLOW);
}

/* ------------------------------------------------------------------ */
/*  Decoder tests                                                      */
/* ------------------------------------------------------------------ */

TEST(test_decode_int)
{
    const uint8_t data[] = "i42e";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 4), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 42);
    /* no more tokens */
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_END);
}

TEST(test_decode_negative_int)
{
    const uint8_t data[] = "i-7e";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 4), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, -7);
}

TEST(test_decode_string)
{
    const uint8_t data[] = "3:foo";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 5), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    ASSERT_EQ(tok.value.string.len, 3);
    ASSERT_TRUE(memcmp(tok.value.string.data, "foo", 3) == 0);
}

TEST(test_decode_list)
{
    /* l3:fooi42ee */
    const uint8_t data[] = "l3:fooi42ee";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 11), BENCODE_OK);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_BEGIN);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    ASSERT_EQ(tok.value.string.len, 3);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 42);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_END);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_END);
}

TEST(test_decode_nested)
{
    /* lli1ei2eeli3eee = list(list(1,2), list(3)) */
    const uint8_t data[] = "lli1ei2eeli3eee";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 15), BENCODE_OK);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_BEGIN);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_BEGIN);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 1);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 2);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_END);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_BEGIN);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 3);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_END);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_LIST_END);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_END);
}

TEST(test_roundtrip)
{
    /* Encode d3:agei25e4:name3:bobe then decode and verify */
    uint8_t buf[64];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_dict_begin(&enc), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"age", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_int(&enc, 25), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"name", 4),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, (const uint8_t *)"bob", 3),
              BENCODE_OK);
    ASSERT_EQ(bencode_encode_dict_end(&enc), BENCODE_OK);

    /* Decode it back */
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, buf, bencode_encoder_len(&enc)),
              BENCODE_OK);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_DICT_BEGIN);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    ASSERT_EQ(tok.value.string.len, 3);
    ASSERT_TRUE(memcmp(tok.value.string.data, "age", 3) == 0);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_INT);
    ASSERT_EQ(tok.value.integer, 25);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    ASSERT_EQ(tok.value.string.len, 4);
    ASSERT_TRUE(memcmp(tok.value.string.data, "name", 4) == 0);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_STRING);
    ASSERT_EQ(tok.value.string.len, 3);
    ASSERT_TRUE(memcmp(tok.value.string.data, "bob", 3) == 0);

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_OK);
    ASSERT_EQ(tok.type, BENCODE_TOKEN_DICT_END); /* dict end = 'e' */

    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_END);
}

TEST(test_decode_invalid_leading_zero)
{
    /* i03e is invalid (leading zero) */
    const uint8_t data[] = "i03e";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 4), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_MALFORMED);
}

TEST(test_decode_invalid_negative_zero)
{
    /* i-0e is invalid */
    const uint8_t data[] = "i-0e";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 4), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_MALFORMED);
}

TEST(test_decode_truncated_string)
{
    /* string says length 5 but only 2 bytes follow */
    const uint8_t data[] = "5:ab";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 4), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_MALFORMED);
}

TEST(test_decode_invalid_char)
{
    const uint8_t data[] = "x";
    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(&dec, data, 1), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, &tok), BENCODE_ERR_MALFORMED);
}

TEST(test_null_params)
{
    bencode_encoder_t enc;
    uint8_t buf[16];
    ASSERT_EQ(bencode_encoder_init(NULL, buf, 16), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_encoder_init(&enc, NULL, 16), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_encode_int(NULL, 1), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_encode_string(NULL, buf, 1), BENCODE_ERR_PARAM);

    bencode_decoder_t dec;
    bencode_token_t tok;
    ASSERT_EQ(bencode_decoder_init(NULL, buf, 1), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_decoder_init(&dec, NULL, 1), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_decode_next(NULL, &tok), BENCODE_ERR_PARAM);
    ASSERT_EQ(bencode_decoder_init(&dec, buf, 1), BENCODE_OK);
    ASSERT_EQ(bencode_decode_next(&dec, NULL), BENCODE_ERR_PARAM);
}

TEST(test_encode_string_nonzero_len_null_data)
{
    uint8_t buf[16];
    bencode_encoder_t enc;
    ASSERT_EQ(bencode_encoder_init(&enc, buf, sizeof(buf)), BENCODE_OK);
    ASSERT_EQ(bencode_encode_string(&enc, NULL, 5), BENCODE_ERR_PARAM);
}

TEST_MAIN("bencode")
    RUN(test_encode_int_positive);
    RUN(test_encode_int_zero);
    RUN(test_encode_int_negative);
    RUN(test_encode_string);
    RUN(test_encode_empty_string);
    RUN(test_encode_list);
    RUN(test_encode_dict);
    RUN(test_encode_overflow);
    RUN(test_decode_int);
    RUN(test_decode_negative_int);
    RUN(test_decode_string);
    RUN(test_decode_list);
    RUN(test_decode_nested);
    RUN(test_roundtrip);
    RUN(test_decode_invalid_leading_zero);
    RUN(test_decode_invalid_negative_zero);
    RUN(test_decode_truncated_string);
    RUN(test_decode_invalid_char);
    RUN(test_null_params);
    RUN(test_encode_string_nonzero_len_null_data);
TEST_MAIN_END()
