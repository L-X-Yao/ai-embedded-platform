// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "cbor_lite.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Encoder tests                                                     */
/* ------------------------------------------------------------------ */

TEST(test_enc_uint_small)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 23), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 2);
    ASSERT_EQ_HEX(buf[0], 0x00);  /* uint 0 */
    ASSERT_EQ_HEX(buf[1], 0x17);  /* uint 23 */
}

TEST(test_enc_uint_onebyte)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 24), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 2);
    ASSERT_EQ_HEX(buf[0], 0x18);  /* uint, 1-byte follows */
    ASSERT_EQ_HEX(buf[1], 24);
}

TEST(test_enc_uint_twobyte)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 1000), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 3);
    ASSERT_EQ_HEX(buf[0], 0x19);           /* uint, 2-byte follows */
    ASSERT_EQ_HEX(buf[1], 0x03);
    ASSERT_EQ_HEX(buf[2], 0xE8);           /* 1000 = 0x03E8 */
}

TEST(test_enc_nint)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    /* encode -1 (raw val 0) and -100 (raw val 99) */
    ASSERT_EQ(cbor_lite_enc_nint(&enc, 0), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_nint(&enc, 99), CBOR_LITE_OK);
    ASSERT_EQ_HEX(buf[0], 0x20);           /* nint(-1) */
    ASSERT_EQ_HEX(buf[1], 0x38);           /* nint, 1-byte follows */
    ASSERT_EQ_HEX(buf[2], 99);             /* raw 99 => -100 */
}

TEST(test_enc_bstr)
{
    uint8_t buf[32];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(cbor_lite_enc_bstr(&enc, data, 4), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 5);
    ASSERT_EQ_HEX(buf[0], 0x44);           /* bstr, length 4 */
    ASSERT_EQ_HEX(buf[1], 0xDE);
    ASSERT_EQ_HEX(buf[4], 0xEF);
}

TEST(test_enc_tstr)
{
    uint8_t buf[32];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_tstr(&enc, "IETF", 4), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 5);
    ASSERT_EQ_HEX(buf[0], 0x64);           /* tstr, length 4 */
    ASSERT_TRUE(memcmp(&buf[1], "IETF", 4) == 0);
}

TEST(test_enc_array_map)
{
    uint8_t buf[32];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_array(&enc, 3), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 1), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 2), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 3), CBOR_LITE_OK);
    ASSERT_EQ_HEX(buf[0], 0x83);           /* array(3) */
    ASSERT_EQ_HEX(buf[1], 0x01);
    ASSERT_EQ_HEX(buf[2], 0x02);
    ASSERT_EQ_HEX(buf[3], 0x03);

    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_map(&enc, 1), CBOR_LITE_OK);
    ASSERT_EQ_HEX(buf[0], 0xA1);           /* map(1) */
}

TEST(test_enc_bool_null)
{
    uint8_t buf[8];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_bool(&enc, true), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_bool(&enc, false), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_null(&enc), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 3);
    ASSERT_EQ_HEX(buf[0], 0xF5);           /* true  */
    ASSERT_EQ_HEX(buf[1], 0xF4);           /* false */
    ASSERT_EQ_HEX(buf[2], 0xF6);           /* null  */
}

TEST(test_enc_float)
{
    uint8_t buf[8];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_float(&enc, 1.0f), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 5);
    ASSERT_EQ_HEX(buf[0], 0xFA);           /* float32 indicator */
    /* IEEE 754: 1.0f = 0x3F800000 */
    ASSERT_EQ_HEX(buf[1], 0x3F);
    ASSERT_EQ_HEX(buf[2], 0x80);
    ASSERT_EQ_HEX(buf[3], 0x00);
    ASSERT_EQ_HEX(buf[4], 0x00);
}

TEST(test_enc_overflow)
{
    uint8_t buf[2];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0), CBOR_LITE_OK);
    /* buffer full — next write must fail */
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0), CBOR_LITE_ERR_OVERFLOW);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 0);  /* overflow → len returns 0 */
}

TEST(test_enc_null_param)
{
    ASSERT_EQ(cbor_lite_enc_uint(NULL, 0), CBOR_LITE_ERR_PARAM);
    ASSERT_EQ(cbor_lite_enc_bstr(NULL, NULL, 0), CBOR_LITE_ERR_PARAM);

    uint8_t buf[8];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    /* non-zero len with NULL data pointer */
    ASSERT_EQ(cbor_lite_enc_bstr(&enc, NULL, 5), CBOR_LITE_ERR_PARAM);
}

/* ------------------------------------------------------------------ */
/*  Decoder tests                                                     */
/* ------------------------------------------------------------------ */

TEST(test_dec_uint)
{
    /* 0x19 0x03 0xE8 = uint(1000) */
    const uint8_t data[] = {0x19, 0x03, 0xE8};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
    ASSERT_EQ(item.uval, 1000);
    ASSERT_EQ(cbor_lite_dec_offset(&dec), 3);
}

TEST(test_dec_nint)
{
    /* 0x38 0x63 = nint(99) => -100 */
    const uint8_t data[] = {0x38, 0x63};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_NINT);
    ASSERT_EQ(item.uval, 99);  /* raw value; actual = -1 - 99 = -100 */
}

TEST(test_dec_tstr)
{
    /* 0x64 "IETF" */
    const uint8_t data[] = {0x64, 'I', 'E', 'T', 'F'};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_TSTR);
    ASSERT_EQ(item.str_len, 4);
    ASSERT_TRUE(memcmp(item.str, "IETF", 4) == 0);
}

TEST(test_dec_bool_null)
{
    const uint8_t data[] = {0xF5, 0xF4, 0xF6};  /* true, false, null */
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_SIMPLE);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_TRUE);
    ASSERT_TRUE(item.bool_val);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_FALSE);
    ASSERT_FALSE(item.bool_val);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_NULL);
}

TEST(test_dec_float)
{
    /* 0xFA 0x41 0xC8 0x00 0x00 = float32(25.0) */
    const uint8_t data[] = {0xFA, 0x41, 0xC8, 0x00, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_SIMPLE);
    ASSERT_NEAR(item.float_val, 25.0f, 0.001f);
}

TEST(test_roundtrip)
{
    uint8_t buf[64];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));

    /* Encode: {"temp": 25, "ok": true} */
    cbor_lite_enc_map(&enc, 2);
    cbor_lite_enc_tstr(&enc, "temp", 4);
    cbor_lite_enc_uint(&enc, 25);
    cbor_lite_enc_tstr(&enc, "ok", 2);
    cbor_lite_enc_bool(&enc, true);

    size_t len = cbor_lite_enc_len(&enc);
    ASSERT_TRUE(len > 0);

    /* Decode and verify */
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, buf, len);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_MAP);
    ASSERT_EQ(item.uval, 2);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_TSTR);
    ASSERT_EQ(item.str_len, 4);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
    ASSERT_EQ(item.uval, 25);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_TSTR);
    ASSERT_EQ(item.str_len, 2);

    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_SIMPLE);
    ASSERT_TRUE(item.bool_val);

    /* All consumed */
    ASSERT_EQ(cbor_lite_dec_offset(&dec), len);
}

TEST(test_dec_short_input)
{
    /* Truncated 2-byte uint header */
    const uint8_t data[] = {0x19, 0x03};  /* needs 2 more bytes */
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
}

TEST(test_dec_null_param)
{
    cbor_lite_item_t item;
    ASSERT_EQ(cbor_lite_dec_next(NULL, &item), CBOR_LITE_ERR_PARAM);

    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, (const uint8_t *)"\x00", 1);
    ASSERT_EQ(cbor_lite_dec_next(&dec, NULL), CBOR_LITE_ERR_PARAM);
}

TEST(test_dec_malformed_reserved_ai)
{
    /* Additional info 28 (0x1C) is reserved in CBOR — decoder must reject.
     * Byte layout: major 0 (uint) | ai 28 => 0x1C */
    const uint8_t data[] = {0x1C};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
}

TEST(test_dec_malformed_indefinite_length)
{
    /* Additional info 31 (0x1F) = indefinite length — not supported.
     * Byte layout: major 2 (bstr) | ai 31 => 0x5F */
    const uint8_t data[] = {0x5F};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
}

TEST_MAIN("cbor_lite")
    RUN(test_enc_uint_small);
    RUN(test_enc_uint_onebyte);
    RUN(test_enc_uint_twobyte);
    RUN(test_enc_nint);
    RUN(test_enc_bstr);
    RUN(test_enc_tstr);
    RUN(test_enc_array_map);
    RUN(test_enc_bool_null);
    RUN(test_enc_float);
    RUN(test_enc_overflow);
    RUN(test_enc_null_param);
    RUN(test_dec_uint);
    RUN(test_dec_nint);
    RUN(test_dec_tstr);
    RUN(test_dec_bool_null);
    RUN(test_dec_float);
    RUN(test_roundtrip);
    RUN(test_dec_short_input);
    RUN(test_dec_null_param);
    RUN(test_dec_malformed_reserved_ai);
    RUN(test_dec_malformed_indefinite_length);
TEST_MAIN_END()
