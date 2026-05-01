#include "test_harness.h"
#include "tlv_packet.h"
#include <string.h>

/* ---- Encoder tests ---- */

TEST(test_encode_u8)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u8(&enc, 0x01, 0xAB), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 3);
    /* tag=0x01 len=0x01 val=0xAB */
    ASSERT_EQ_HEX(buf[0], 0x01);
    ASSERT_EQ_HEX(buf[1], 0x01);
    ASSERT_EQ_HEX(buf[2], 0xAB);
}

TEST(test_encode_u16)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u16(&enc, 0x02, 0x1234), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 4);
    ASSERT_EQ_HEX(buf[0], 0x02);
    ASSERT_EQ_HEX(buf[1], 0x02);
    ASSERT_EQ_HEX(buf[2], 0x12);
    ASSERT_EQ_HEX(buf[3], 0x34);
}

TEST(test_encode_u32)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u32(&enc, 0x03, 0xDEADBEEF), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 6);
    ASSERT_EQ_HEX(buf[0], 0x03);
    ASSERT_EQ_HEX(buf[1], 0x04);
    ASSERT_EQ_HEX(buf[2], 0xDE);
    ASSERT_EQ_HEX(buf[3], 0xAD);
    ASSERT_EQ_HEX(buf[4], 0xBE);
    ASSERT_EQ_HEX(buf[5], 0xEF);
}

TEST(test_encode_string)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_string(&enc, 0x04, "Hi"), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 4);
    ASSERT_EQ_HEX(buf[0], 0x04);
    ASSERT_EQ_HEX(buf[1], 0x02);
    ASSERT_EQ(buf[2], 'H');
    ASSERT_EQ(buf[3], 'i');
}

TEST(test_encode_raw)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    uint8_t val[] = {0xCA, 0xFE};
    ASSERT_EQ(tlv_encode(&enc, 0x05, val, 2), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 4);
    ASSERT_EQ_HEX(buf[2], 0xCA);
    ASSERT_EQ_HEX(buf[3], 0xFE);
}

/* ---- Decoder tests ---- */

TEST(test_decode_single)
{
    /* Manually build: tag=0x10, len=0x02, val={0xAA,0xBB} */
    uint8_t wire[] = {0x10, 0x02, 0xAA, 0xBB};
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, wire, sizeof(wire));
    ASSERT_FALSE(tlv_decoder_done(&dec));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x10);
    ASSERT_EQ(tlv.length, 2);
    ASSERT_EQ_HEX(tlv.value[0], 0xAA);
    ASSERT_EQ_HEX(tlv.value[1], 0xBB);
    ASSERT_TRUE(tlv_decoder_done(&dec));
}

TEST(test_decode_multiple)
{
    /* Two TLVs: {tag=1,len=1,val=0x11} {tag=2,len=1,val=0x22} */
    uint8_t wire[] = {0x01, 0x01, 0x11, 0x02, 0x01, 0x22};
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, wire, sizeof(wire));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 1);
    ASSERT_EQ(tlv.length, 1);
    ASSERT_EQ_HEX(tlv.value[0], 0x11);

    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 2);
    ASSERT_EQ(tlv.length, 1);
    ASSERT_EQ_HEX(tlv.value[0], 0x22);

    ASSERT_TRUE(tlv_decoder_done(&dec));
}

/* ---- Roundtrip ---- */

TEST(test_roundtrip)
{
    uint8_t buf[64];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u8(&enc, 0x01, 0x42), TLV_OK);
    ASSERT_EQ(tlv_encode_u16(&enc, 0x02, 0x1234), TLV_OK);
    ASSERT_EQ(tlv_encode_string(&enc, 0x03, "abc"), TLV_OK);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));

    tlv_t tlv;
    /* entry 1: u8 */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x01);
    ASSERT_EQ(tlv.length, 1);
    ASSERT_EQ_HEX(tlv.value[0], 0x42);

    /* entry 2: u16 */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x02);
    ASSERT_EQ(tlv.length, 2);
    ASSERT_EQ_HEX(tlv.value[0], 0x12);
    ASSERT_EQ_HEX(tlv.value[1], 0x34);

    /* entry 3: string */
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x03);
    ASSERT_EQ(tlv.length, 3);
    ASSERT_EQ(memcmp(tlv.value, "abc", 3), 0);

    ASSERT_TRUE(tlv_decoder_done(&dec));
}

/* ---- Find ---- */

TEST(test_find_hit)
{
    uint8_t buf[64];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0A, 0x11), TLV_OK);
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0B, 0x22), TLV_OK);
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0C, 0x33), TLV_OK);

    tlv_t result;
    ASSERT_EQ(tlv_find(buf, tlv_encoder_len(&enc), 0x0B, &result), TLV_OK);
    ASSERT_EQ(result.tag, 0x0B);
    ASSERT_EQ(result.length, 1);
    ASSERT_EQ_HEX(result.value[0], 0x22);
}

TEST(test_find_miss)
{
    uint8_t buf[64];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0A, 0x11), TLV_OK);

    tlv_t result;
    ASSERT_EQ(tlv_find(buf, tlv_encoder_len(&enc), 0xFF, &result), TLV_ERR_FORMAT);
}

/* ---- Overflow ---- */

TEST(test_encode_overflow)
{
    uint8_t buf[4];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    /* tag(1) + len(1) + val(4) = 6 > 4 */
    ASSERT_EQ(tlv_encode_u32(&enc, 0x01, 0x12345678), TLV_ERR_OVERFLOW);
    /* encoder pos unchanged */
    ASSERT_EQ(tlv_encoder_len(&enc), 0);
}

/* ---- Truncated input ---- */

TEST(test_decode_truncated)
{
    /* tag=0x01 len=0x05 but only 2 value bytes */
    uint8_t wire[] = {0x01, 0x05, 0xAA, 0xBB};
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, wire, sizeof(wire));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_TRUNCATED);
}

/* ---- Empty buffer ---- */

TEST(test_decode_empty)
{
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, (const uint8_t *)"", 0);
    ASSERT_TRUE(tlv_decoder_done(&dec));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_ERR_TRUNCATED);
}

/* ---- 2-byte tag ---- */

TEST(test_two_byte_tag)
{
    uint8_t buf[32];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    /* tag 0x0100 >= 0x80 so needs 2-byte encoding */
    ASSERT_EQ(tlv_encode_u8(&enc, 0x0100, 0x55), TLV_OK);
    /* 2-byte tag + 1-byte len + 1-byte val = 4 */
    ASSERT_EQ(tlv_encoder_len(&enc), 4);
    /* first byte: 0x80 | (0x0100 >> 8) = 0x81 */
    ASSERT_EQ_HEX(buf[0], 0x81);
    ASSERT_EQ_HEX(buf[1], 0x00);
    ASSERT_EQ_HEX(buf[2], 0x01);
    ASSERT_EQ_HEX(buf[3], 0x55);

    /* decode it back */
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x0100);
    ASSERT_EQ(tlv.length, 1);
    ASSERT_EQ_HEX(tlv.value[0], 0x55);
}

/* ---- 2-byte length (value > 127 bytes) ---- */

TEST(test_two_byte_length)
{
    /* 200-byte value needs 2-byte length encoding */
    uint8_t buf[256];
    uint8_t val[200];
    memset(val, 0xBB, sizeof(val));

    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode(&enc, 0x07, val, 200), TLV_OK);
    /* 1-byte tag + 2-byte len + 200-byte val = 203 */
    ASSERT_EQ(tlv_encoder_len(&enc), 203);
    /* length byte 0: 0x80 | (200 >> 8) = 0x80, byte 1: 200 & 0xFF = 0xC8 */
    ASSERT_EQ_HEX(buf[1], 0x80);
    ASSERT_EQ_HEX(buf[2], 0xC8);

    /* decode it back */
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x07);
    ASSERT_EQ(tlv.length, 200);
    ASSERT_EQ(tlv.value[0], 0xBB);
    ASSERT_EQ(tlv.value[199], 0xBB);
}

/* ---- NULL params ---- */

TEST(test_null_params)
{
    ASSERT_EQ(tlv_encode(NULL, 0, NULL, 0), TLV_ERR_PARAMS);
    ASSERT_EQ(tlv_encode_string(NULL, 0, NULL), TLV_ERR_PARAMS);
    ASSERT_EQ(tlv_decode_next(NULL, NULL), TLV_ERR_PARAMS);
    ASSERT_EQ(tlv_find(NULL, 0, 0, NULL), TLV_ERR_PARAMS);
    ASSERT_TRUE(tlv_decoder_done(NULL));
    ASSERT_EQ(tlv_encoder_len(NULL), 0);

    /* encoder with NULL buf */
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, NULL, 0);
    ASSERT_EQ(tlv_encode_u8(&enc, 0x01, 0x42), TLV_ERR_PARAMS);
}

/* ---- decoder_done ---- */

TEST(test_decoder_done)
{
    uint8_t wire[] = {0x01, 0x01, 0xFF};
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, wire, sizeof(wire));
    ASSERT_FALSE(tlv_decoder_done(&dec));

    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_TRUE(tlv_decoder_done(&dec));
}

/* ---- Nested TLV (TLV within value) ---- */

TEST(test_nested_tlv)
{
    /* Encode inner TLV */
    uint8_t inner_buf[32];
    tlv_encoder_t inner_enc;
    tlv_encoder_init(&inner_enc, inner_buf, sizeof(inner_buf));
    ASSERT_EQ(tlv_encode_u8(&inner_enc, 0x10, 0xAA), TLV_OK);
    ASSERT_EQ(tlv_encode_u8(&inner_enc, 0x11, 0xBB), TLV_OK);

    /* Encode outer TLV whose value is the inner stream */
    uint8_t outer_buf[64];
    tlv_encoder_t outer_enc;
    tlv_encoder_init(&outer_enc, outer_buf, sizeof(outer_buf));
    ASSERT_EQ(tlv_encode(&outer_enc, 0x01, inner_buf,
                         tlv_encoder_len(&inner_enc)), TLV_OK);

    /* Decode outer */
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, outer_buf, tlv_encoder_len(&outer_enc));
    tlv_t outer;
    ASSERT_EQ(tlv_decode_next(&dec, &outer), TLV_OK);
    ASSERT_EQ(outer.tag, 0x01);
    ASSERT_EQ(outer.length, tlv_encoder_len(&inner_enc));

    /* Decode inner from outer's value */
    tlv_decoder_t inner_dec;
    tlv_decoder_init(&inner_dec, outer.value, outer.length);

    tlv_t t1, t2;
    ASSERT_EQ(tlv_decode_next(&inner_dec, &t1), TLV_OK);
    ASSERT_EQ(t1.tag, 0x10);
    ASSERT_EQ_HEX(t1.value[0], 0xAA);

    ASSERT_EQ(tlv_decode_next(&inner_dec, &t2), TLV_OK);
    ASSERT_EQ(t2.tag, 0x11);
    ASSERT_EQ_HEX(t2.value[0], 0xBB);

    ASSERT_TRUE(tlv_decoder_done(&inner_dec));
}

/* ---- Zero-length value ---- */

TEST(test_encode_zero_length)
{
    uint8_t buf[16];
    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, sizeof(buf));
    ASSERT_EQ(tlv_encode(&enc, 0x05, NULL, 0), TLV_OK);
    ASSERT_EQ(tlv_encoder_len(&enc), 2);
    ASSERT_EQ_HEX(buf[0], 0x05);
    ASSERT_EQ_HEX(buf[1], 0x00);

    /* decode zero-length */
    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    tlv_t tlv;
    ASSERT_EQ(tlv_decode_next(&dec, &tlv), TLV_OK);
    ASSERT_EQ(tlv.tag, 0x05);
    ASSERT_EQ(tlv.length, 0);
    ASSERT_TRUE(tlv.value == NULL);
}

TEST_MAIN("tlv_packet")
    RUN(test_encode_u8);
    RUN(test_encode_u16);
    RUN(test_encode_u32);
    RUN(test_encode_string);
    RUN(test_encode_raw);
    RUN(test_decode_single);
    RUN(test_decode_multiple);
    RUN(test_roundtrip);
    RUN(test_find_hit);
    RUN(test_find_miss);
    RUN(test_encode_overflow);
    RUN(test_decode_truncated);
    RUN(test_decode_empty);
    RUN(test_two_byte_tag);
    RUN(test_two_byte_length);
    RUN(test_null_params);
    RUN(test_decoder_done);
    RUN(test_nested_tlv);
    RUN(test_encode_zero_length);
TEST_MAIN_END()
