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

/* ------------------------------------------------------------------ */
/*  enc_head 4-byte path: values in 0x10000..0xFFFFFFFF              */
/* ------------------------------------------------------------------ */

TEST(test_enc_uint_fourbyte)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    /* 0x00010000 = 65536 — first value requiring 4-byte argument */
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 65536U), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 5);
    ASSERT_EQ_HEX(buf[0], 0x1A);           /* major 0, ai 26 */
    ASSERT_EQ_HEX(buf[1], 0x00);
    ASSERT_EQ_HEX(buf[2], 0x01);
    ASSERT_EQ_HEX(buf[3], 0x00);
    ASSERT_EQ_HEX(buf[4], 0x00);
}

TEST(test_enc_uint_fourbyte_max)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0xFFFFFFFFULL), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 5);
    ASSERT_EQ_HEX(buf[0], 0x1A);
    ASSERT_EQ_HEX(buf[1], 0xFF);
    ASSERT_EQ_HEX(buf[2], 0xFF);
    ASSERT_EQ_HEX(buf[3], 0xFF);
    ASSERT_EQ_HEX(buf[4], 0xFF);
}

TEST(test_enc_overflow_fourbyte)
{
    /* Buffer of 4 bytes — too small for 5-byte encoding */
    uint8_t buf[4];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 65536U), CBOR_LITE_ERR_OVERFLOW);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 0);
}

/* ------------------------------------------------------------------ */
/*  enc_head 8-byte path: values > 0xFFFFFFFF                        */
/* ------------------------------------------------------------------ */

TEST(test_enc_uint_eightbyte)
{
    uint8_t buf[16];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    /* 0x100000000 = 2^32 — first value requiring 8-byte argument */
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0x100000000ULL), CBOR_LITE_OK);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 9);
    ASSERT_EQ_HEX(buf[0], 0x1B);           /* major 0, ai 27 */
    ASSERT_EQ_HEX(buf[1], 0x00);
    ASSERT_EQ_HEX(buf[2], 0x00);
    ASSERT_EQ_HEX(buf[3], 0x00);
    ASSERT_EQ_HEX(buf[4], 0x01);
    ASSERT_EQ_HEX(buf[5], 0x00);
    ASSERT_EQ_HEX(buf[6], 0x00);
    ASSERT_EQ_HEX(buf[7], 0x00);
    ASSERT_EQ_HEX(buf[8], 0x00);
}

TEST(test_enc_overflow_eightbyte)
{
    /* Buffer of 8 bytes — too small for 9-byte encoding */
    uint8_t buf[8];
    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(cbor_lite_enc_uint(&enc, 0x100000000ULL), CBOR_LITE_ERR_OVERFLOW);
    ASSERT_EQ(cbor_lite_enc_len(&enc), 0);
}

/* ------------------------------------------------------------------ */
/*  Decoder: 4-byte argument (ai == 26)                               */
/* ------------------------------------------------------------------ */

TEST(test_dec_uint_fourbyte)
{
    /* 0x1A 0x00 0x01 0x00 0x00 = uint(65536) */
    const uint8_t data[] = {0x1A, 0x00, 0x01, 0x00, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
    ASSERT_EQ(item.uval, 65536U);
    ASSERT_EQ(cbor_lite_dec_offset(&dec), 5);
}

TEST(test_dec_short_fourbyte)
{
    /* Truncated 4-byte argument: only 3 bytes available */
    const uint8_t data[] = {0x1A, 0x00, 0x01, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
}

/* ------------------------------------------------------------------ */
/*  Decoder: 8-byte argument (ai == 27)                               */
/* ------------------------------------------------------------------ */

TEST(test_dec_uint_eightbyte)
{
    /* 0x1B followed by 8 bytes = uint(0x100000000) */
    const uint8_t data[] = {0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_UINT);
    ASSERT_EQ(item.uval, 0x100000000ULL);
    ASSERT_EQ(cbor_lite_dec_offset(&dec), 9);
}

TEST(test_dec_short_eightbyte)
{
    /* Truncated 8-byte argument: only 7 bytes available */
    const uint8_t data[] = {0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
}

/* ------------------------------------------------------------------ */
/*  Decoder: bstr (major type 2)                                      */
/* ------------------------------------------------------------------ */

TEST(test_dec_bstr)
{
    /* 0x44 0xDE 0xAD 0xBE 0xEF = bstr(h'DEADBEEF') */
    const uint8_t data[] = {0x44, 0xDE, 0xAD, 0xBE, 0xEF};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_BSTR);
    ASSERT_EQ(item.str_len, 4);
    ASSERT_EQ_HEX(item.str[0], 0xDE);
    ASSERT_EQ_HEX(item.str[3], 0xEF);
    ASSERT_EQ(cbor_lite_dec_offset(&dec), 5);
}

TEST(test_dec_bstr_empty)
{
    /* 0x40 = bstr(h'') — zero-length byte string */
    const uint8_t data[] = {0x40};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_BSTR);
    ASSERT_EQ(item.str_len, 0);
}

TEST(test_dec_bstr_short)
{
    /* Header says 4 bytes but only 2 bytes available */
    const uint8_t data[] = {0x44, 0xDE, 0xAD};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
}

/* ------------------------------------------------------------------ */
/*  Decoder: tag (major type 6)                                       */
/* ------------------------------------------------------------------ */

TEST(test_dec_tag)
{
    /* 0xC1 = tag(1) — Unix timestamp tag; the tagged value follows */
    const uint8_t data[] = {0xC1, 0x1A, 0x5F, 0x4D, 0x1A, 0x02};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;
    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.major, CBOR_MAJOR_TAG);
    ASSERT_EQ(item.uval, 1U);    /* tag number 1 */
}

/* ------------------------------------------------------------------ */
/*  Major 7 values the decoder must not pass off as simple values     */
/* ------------------------------------------------------------------ */

TEST(test_dec_refuses_float16)
{
    /* RFC 8949 Appendix A: 1.0 is 0xf93c00 */
    const uint8_t one[] = {0xF9, 0x3C, 0x00};
    /* Low byte 0x15 must not read as true */
    const uint8_t low_byte_true[] = {0xF9, 0x00, 0x15};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, one, sizeof(one));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
    cbor_lite_dec_init(&dec, low_byte_true, sizeof(low_byte_true));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
}

TEST(test_dec_refuses_float64)
{
    /* RFC 8949 Appendix A: 1.1 is 0xfb3ff199999999999a */
    const uint8_t one_one[] = {0xFB, 0x3F, 0xF1, 0x99, 0x99,
                               0x99, 0x99, 0x99, 0x9A};
    /* Low byte 0x1A must not read as the float32 marker, 0x15 as true */
    const uint8_t low_byte_float32[] = {0xFB, 0x3F, 0xF0, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x1A};
    const uint8_t low_byte_true[] = {0xFB, 0x3F, 0xF0, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x15};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, one_one, sizeof(one_one));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
    cbor_lite_dec_init(&dec, low_byte_float32, sizeof(low_byte_float32));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
    cbor_lite_dec_init(&dec, low_byte_true, sizeof(low_byte_true));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
}

TEST(test_dec_refused_float_leaves_the_next_item_readable)
{
    /* float16 1.0, then true */
    const uint8_t data[] = {0xF9, 0x3C, 0x00, 0xF5};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
    ASSERT_EQ(cbor_lite_dec_offset(&dec), 3U);
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_TRUE(item.bool_val);
}

TEST(test_dec_short_float16_and_float64_stay_short)
{
    const uint8_t half[] = {0xF9, 0x3C};
    const uint8_t dbl[] = {0xFB, 0x3F, 0xF0, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, half, sizeof(half));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
    cbor_lite_dec_init(&dec, dbl, sizeof(dbl));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_SHORT);
}

TEST(test_dec_refuses_two_byte_simple_below_32)
{
    /* RFC 8949 3.3: 0xf8 followed by a byte below 0x20 is not well-formed */
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    for (unsigned v = 0; v < 32U; v++) {
        const uint8_t data[] = {0xF8, (uint8_t)v};
        cbor_lite_dec_init(&dec, data, sizeof(data));
        ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_ERR_MALFORMED);
    }
}

TEST(test_dec_two_byte_simple_from_32_up)
{
    /* RFC 8949 Appendix A: simple(255) is 0xf8ff */
    const uint8_t lowest[] = {0xF8, 0x20};
    const uint8_t highest[] = {0xF8, 0xFF};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, lowest, sizeof(lowest));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, 32U);
    cbor_lite_dec_init(&dec, highest, sizeof(highest));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, 255U);
}

TEST(test_dec_one_byte_simple_and_float32_still_decode)
{
    /* RFC 8949 Appendix A: 0xf4 false, 0xf5 true, 0xf6 null, 0xf0 simple(16),
       0xfa47c35000 100000.0 */
    const uint8_t data[] = {0xF4, 0xF5, 0xF6, 0xF0, 0xFA, 0x47, 0xC3, 0x50, 0x00};
    cbor_lite_decoder_t dec;
    cbor_lite_item_t item;

    cbor_lite_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_FALSE);
    ASSERT_FALSE(item.bool_val);
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_TRUE);
    ASSERT_TRUE(item.bool_val);
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, CBOR_SIMPLE_NULL);
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, 16U);
    ASSERT_EQ(cbor_lite_dec_next(&dec, &item), CBOR_LITE_OK);
    ASSERT_EQ(item.simple, 26U);
    ASSERT_NEAR(item.float_val, 100000.0f, 0.001f);
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
    RUN(test_enc_uint_fourbyte);
    RUN(test_enc_uint_fourbyte_max);
    RUN(test_enc_overflow_fourbyte);
    RUN(test_enc_uint_eightbyte);
    RUN(test_enc_overflow_eightbyte);
    RUN(test_dec_uint_fourbyte);
    RUN(test_dec_short_fourbyte);
    RUN(test_dec_uint_eightbyte);
    RUN(test_dec_short_eightbyte);
    RUN(test_dec_bstr);
    RUN(test_dec_bstr_empty);
    RUN(test_dec_bstr_short);
    RUN(test_dec_tag);
    RUN(test_dec_refuses_float16);
    RUN(test_dec_refuses_float64);
    RUN(test_dec_refused_float_leaves_the_next_item_readable);
    RUN(test_dec_short_float16_and_float64_stay_short);
    RUN(test_dec_refuses_two_byte_simple_below_32);
    RUN(test_dec_two_byte_simple_from_32_up);
    RUN(test_dec_one_byte_simple_and_float32_still_decode);
TEST_MAIN_END()
