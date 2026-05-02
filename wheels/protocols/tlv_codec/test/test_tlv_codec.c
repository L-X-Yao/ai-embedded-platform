// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "tlv_codec.h"
#include <string.h>

/* ── Encoder tests ───────────────────────────────────────────────── */

TEST(test_encode_u8)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u8(&enc, 0x0001, 0xAB), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 5);
    /* tag=0x0001 big-endian */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x01);
    /* length=0x0001 big-endian */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x01);
    /* value */
    ASSERT_EQ(buf[4], 0xAB);
}

TEST(test_encode_u16)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u16(&enc, 0x0002, 0x1234), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 6);
    ASSERT_EQ(buf[4], 0x12);
    ASSERT_EQ(buf[5], 0x34);
}

TEST(test_encode_u32)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u32(&enc, 0x0003, 0xDEADBEEF), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 8);
    ASSERT_EQ(buf[4], 0xDE);
    ASSERT_EQ(buf[5], 0xAD);
    ASSERT_EQ(buf[6], 0xBE);
    ASSERT_EQ(buf[7], 0xEF);
}

TEST(test_encode_string)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_string(&enc, 0x0010, "Hi"), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 6);
    /* length = 2 (no NUL) */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x02);
    ASSERT_EQ(buf[4], 'H');
    ASSERT_EQ(buf[5], 'i');
}

TEST(test_encode_raw)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    uint8_t payload[] = {0x01, 0x02, 0x03};
    ASSERT_EQ(tlv_encode(&enc, 0x00FF, payload, 3), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 7);
    ASSERT_EQ(buf[4], 0x01);
    ASSERT_EQ(buf[5], 0x02);
    ASSERT_EQ(buf[6], 0x03);
}

TEST(test_encode_empty_value)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode(&enc, 0x0099, NULL, 0), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 4);
    /* length field = 0 */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x00);
}

TEST(test_encoder_overflow)
{
    uint8_t buf[6]; /* only room for header + 2 value bytes */
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    /* 3-byte value needs 7 bytes total — should fail */
    uint8_t payload[] = {0x01, 0x02, 0x03};
    ASSERT_EQ(tlv_encode(&enc, 0x0001, payload, 3), TLV_ERR_OVERFLOW);
    /* encoder position unchanged */
    ASSERT_EQ(tlv_encoder_len(&enc), 0);
}

TEST(test_encode_null_params)
{
    ASSERT_EQ(tlv_encode(NULL, 0x0001, NULL, 0), TLV_ERR_PARAM);

    tlv_encoder_t enc;
    tlv_encoder_init(&enc, NULL, 0);
    ASSERT_EQ(tlv_encode(&enc, 0x0001, NULL, 0), TLV_ERR_PARAM);

    ASSERT_EQ(tlv_encode_string(&enc, 0x0001, NULL), TLV_ERR_PARAM);

    /* non-zero length with NULL value */
    uint8_t buf[32];
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode(&enc, 0x0001, NULL, 5), TLV_ERR_PARAM);
}

/* ── Decoder tests ───────────────────────────────────────────────── */

TEST(test_decode_roundtrip)
{
    uint8_t buf[64];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u32(&enc, 0x0042, 0x12345678), TLV_OK);

    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0042);
    ASSERT_EQ(tlv.length, 4);
    ASSERT_EQ(tlv_get_u32(&tlv), 0x12345678);
    ASSERT_TRUE(tlv_decoder_done(&dec));
}

TEST(test_decode_multiple)
{
    uint8_t buf[64];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u8(&enc, 0x0001, 0xAA), TLV_OK);
    ASSERT_EQ(tlv_encode_u16(&enc, 0x0002, 0xBBCC), TLV_OK);
    ASSERT_EQ(tlv_encode_string(&enc, 0x0003, "OK"), TLV_OK);

    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));

    /* first */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0001);
    ASSERT_EQ(tlv_get_u8(&tlv), 0xAA);

    /* second */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0002);
    ASSERT_EQ(tlv_get_u16(&tlv), 0xBBCC);

    /* third */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0003);
    ASSERT_EQ(tlv.length, 2);
    ASSERT_TRUE(memcmp(tlv.value, "OK", 2) == 0);

    ASSERT_TRUE(tlv_decoder_done(&dec));
}

TEST(test_decoder_truncated)
{
    /* incomplete header */
    uint8_t short_buf[] = {0x00, 0x01, 0x00};
    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, short_buf, 3);
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_TRUNCATED);

    /* header says length=4 but only 2 value bytes present */
    uint8_t trunc_buf[] = {0x00, 0x01, 0x00, 0x04, 0xAA, 0xBB};
    tlv_decoder_init(&dec, trunc_buf, 6);
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_TRUNCATED);
}

TEST(test_decoder_done)
{
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, NULL, 0);
    ASSERT_TRUE(tlv_decoder_done(&dec));

    uint8_t buf[8];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0001, 0x42), TLV_OK);

    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    ASSERT_TRUE(!tlv_decoder_done(&dec));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_TRUE(tlv_decoder_done(&dec));
}

TEST(test_get_helpers)
{
    /* u8 */
    uint8_t v8 = 0x55;
    tlv_t tlv = {.tag = 1, .length = 1, .value = &v8};
    ASSERT_EQ(tlv_get_u8(&tlv), 0x55);

    /* u16 */
    uint8_t v16[] = {0x12, 0x34};
    tlv.length = 2; tlv.value = v16;
    ASSERT_EQ(tlv_get_u16(&tlv), 0x1234);

    /* u32 */
    uint8_t v32[] = {0xCA, 0xFE, 0xBA, 0xBE};
    tlv.length = 4; tlv.value = v32;
    ASSERT_EQ(tlv_get_u32(&tlv), 0xCAFEBABE);

    /* too short returns 0 */
    tlv.length = 0; tlv.value = NULL;
    ASSERT_EQ(tlv_get_u8(&tlv), 0);
    ASSERT_EQ(tlv_get_u16(&tlv), 0);
    ASSERT_EQ(tlv_get_u32(&tlv), 0);

    /* NULL tlv */
    ASSERT_EQ(tlv_get_u8(NULL), 0);
    ASSERT_EQ(tlv_get_u16(NULL), 0);
    ASSERT_EQ(tlv_get_u32(NULL), 0);
}

TEST(test_tag_preservation)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    ASSERT_EQ(tlv_encode_u8(&enc, 0xABCD, 0x00), TLV_OK);

    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0xABCD);
}

TEST(test_large_value)
{
    uint8_t buf[512];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    uint8_t payload[256];
    memset(payload, 0xBB, sizeof(payload));
    ASSERT_EQ(tlv_encode(&enc, 0x0007, payload, 256), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 260);

    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.length, 256);
    ASSERT_TRUE(tlv.value[0] == 0xBB);
    ASSERT_TRUE(tlv.value[255] == 0xBB);
}

TEST(test_encode_then_decode_sequence)
{
    uint8_t buf[128];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));

    /* build a mixed sequence */
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0001, 42), TLV_OK);
    ASSERT_EQ(tlv_encode_u16(&enc, 0x0002, 1000), TLV_OK);
    ASSERT_EQ(tlv_encode_u32(&enc, 0x0003, 0x0000FFFF), TLV_OK);
    ASSERT_EQ(tlv_encode_string(&enc, 0x0004, "test"), TLV_OK);
    ASSERT_EQ(tlv_encode(&enc, 0x0005, NULL, 0), TLV_OK);

    /* decode all back */
    tlv_decoder_t dec;
    tlv_t tlv;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0001);
    ASSERT_EQ(tlv_get_u8(&tlv), 42);

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0002);
    ASSERT_EQ(tlv_get_u16(&tlv), 1000);

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0003);
    ASSERT_EQ(tlv_get_u32(&tlv), 0x0000FFFF);

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0004);
    ASSERT_EQ(tlv.length, 4);
    ASSERT_TRUE(memcmp(tlv.value, "test", 4) == 0);

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0005);
    ASSERT_EQ(tlv.length, 0);
    ASSERT_TRUE(tlv.value == NULL);

    ASSERT_TRUE(tlv_decoder_done(&dec));

    /* one more decode should return truncated */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_TRUNCATED);
}

TEST(test_decode_null_params)
{
    ASSERT_EQ(tlv_decode_next(NULL, NULL), TLV_ERR_PARAM);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, NULL, 0);
    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_PARAM);
}

TEST_MAIN("tlv_codec")
    RUN(test_encode_u8);
    RUN(test_encode_u16);
    RUN(test_encode_u32);
    RUN(test_encode_string);
    RUN(test_encode_raw);
    RUN(test_encode_empty_value);
    RUN(test_encoder_overflow);
    RUN(test_encode_null_params);
    RUN(test_decode_roundtrip);
    RUN(test_decode_multiple);
    RUN(test_decoder_truncated);
    RUN(test_decoder_done);
    RUN(test_get_helpers);
    RUN(test_tag_preservation);
    RUN(test_large_value);
    RUN(test_encode_then_decode_sequence);
    RUN(test_decode_null_params);
TEST_MAIN_END()
