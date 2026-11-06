// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "varint.h"
#include <string.h>

/* ---------- encode u64 ---------- */

TEST(test_encode_zero)
{
    uint8_t buf[10];
    size_t len = 0;
    ASSERT_EQ(varint_encode_u64(0, buf, sizeof(buf), &len), VARINT_OK);
    ASSERT_EQ(len, 1u);
    ASSERT_EQ(buf[0], 0x00);
}

TEST(test_encode_127)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_u64(127, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 1u);
    ASSERT_EQ(buf[0], 0x7F);
}

TEST(test_encode_128)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_u64(128, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 2u);
    ASSERT_EQ(buf[0], 0x80);
    ASSERT_EQ(buf[1], 0x01);
}

TEST(test_encode_300)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_u64(300, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 2u);
    ASSERT_EQ(buf[0], 0xAC);
    ASSERT_EQ(buf[1], 0x02);
}

TEST(test_encode_max_u64)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_u64(UINT64_MAX, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 10u);
}

TEST(test_encode_overflow)
{
    uint8_t buf[1];
    size_t len = 0;
    ASSERT_EQ(varint_encode_u64(128, buf, 1, &len), VARINT_ERR_OVERFLOW);
}

TEST(test_encode_null)
{
    size_t len;
    ASSERT_EQ(varint_encode_u64(0, NULL, 10, &len), VARINT_ERR_PARAM);
    uint8_t buf[10];
    ASSERT_EQ(varint_encode_u64(0, buf, 10, NULL), VARINT_ERR_PARAM);
}

/* ---------- decode u64 ---------- */

TEST(test_decode_zero)
{
    uint8_t buf[] = {0x00};
    uint64_t val = 99;
    size_t len = 0;
    ASSERT_EQ(varint_decode_u64(buf, 1, &val, &len), VARINT_OK);
    ASSERT_EQ(val, 0u);
    ASSERT_EQ(len, 1u);
}

TEST(test_decode_300)
{
    uint8_t buf[] = {0xAC, 0x02};
    uint64_t val = 0;
    size_t len = 0;
    varint_decode_u64(buf, 2, &val, &len);
    ASSERT_EQ(val, 300u);
    ASSERT_EQ(len, 2u);
}

TEST(test_decode_short)
{
    uint8_t buf[] = {0x80};
    uint64_t val;
    size_t len;
    ASSERT_EQ(varint_decode_u64(buf, 1, &val, &len), VARINT_ERR_SHORT);
}

TEST(test_decode_null)
{
    uint64_t val;
    size_t len;
    ASSERT_EQ(varint_decode_u64(NULL, 1, &val, &len), VARINT_ERR_PARAM);
    uint8_t buf[] = {0};
    ASSERT_EQ(varint_decode_u64(buf, 1, NULL, &len), VARINT_ERR_PARAM);
    ASSERT_EQ(varint_decode_u64(buf, 1, &val, NULL), VARINT_ERR_PARAM);
}

/* ---------- roundtrip u64 ---------- */

TEST(test_roundtrip_u64)
{
    uint64_t values[] = {0, 1, 127, 128, 255, 256, 16383, 16384, 0xFFFFFFFF, UINT64_MAX};
    for (int i = 0; i < (int)(sizeof(values)/sizeof(values[0])); i++) {
        uint8_t buf[10];
        size_t elen = 0, dlen = 0;
        uint64_t decoded = 0;
        varint_encode_u64(values[i], buf, sizeof(buf), &elen);
        varint_decode_u64(buf, elen, &decoded, &dlen);
        ASSERT_EQ(decoded, values[i]);
        ASSERT_EQ(elen, dlen);
    }
}

/* ---------- signed (ZigZag) ---------- */

TEST(test_zigzag_roundtrip)
{
    int64_t values[] = {0, -1, 1, -2, 2, -64, 63, -128, 127, INT64_MIN, INT64_MAX};
    for (int i = 0; i < (int)(sizeof(values)/sizeof(values[0])); i++) {
        uint8_t buf[10];
        size_t elen = 0, dlen = 0;
        int64_t decoded = 0;
        varint_encode_s64(values[i], buf, sizeof(buf), &elen);
        varint_decode_s64(buf, elen, &decoded, &dlen);
        ASSERT_EQ(decoded, values[i]);
    }
}

TEST(test_zigzag_zero)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_s64(0, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 1u);
    ASSERT_EQ(buf[0], 0x00);
}

TEST(test_zigzag_minus1)
{
    uint8_t buf[10];
    size_t len = 0;
    varint_encode_s64(-1, buf, sizeof(buf), &len);
    ASSERT_EQ(len, 1u);
    ASSERT_EQ(buf[0], 0x01);
}

TEST(test_decode_s64_null)
{
    int64_t val;
    size_t len;
    ASSERT_EQ(varint_decode_s64(NULL, 1, &val, &len), VARINT_ERR_PARAM);
    uint8_t buf[] = {0};
    ASSERT_EQ(varint_decode_s64(buf, 1, NULL, &len), VARINT_ERR_PARAM);
}

/* ---------- encoded_size ---------- */

TEST(test_encoded_size)
{
    ASSERT_EQ(varint_encoded_size_u64(0), 1u);
    ASSERT_EQ(varint_encoded_size_u64(127), 1u);
    ASSERT_EQ(varint_encoded_size_u64(128), 2u);
    ASSERT_EQ(varint_encoded_size_u64(16383), 2u);
    ASSERT_EQ(varint_encoded_size_u64(16384), 3u);
    ASSERT_EQ(varint_encoded_size_u64(UINT64_MAX), 10u);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)VARINT_OK, 0);
    ASSERT_EQ((int)VARINT_ERR_PARAM, -1);
    ASSERT_EQ((int)VARINT_ERR_OVERFLOW, -2);
    ASSERT_EQ((int)VARINT_ERR_SHORT, -3);
}

/* ---------- decode past 64 bits ---------- */

/* Nine bytes carry 63 bits, so the tenth byte may only hold bit 63. */
static void fill_nine(uint8_t *buf, uint8_t head, uint8_t last)
{
    memset(buf, head, 9);
    buf[9] = last;
}

TEST(test_decode_tenth_byte_top_values)
{
    uint8_t buf[10];
    uint64_t val = 0;
    size_t len = 0;

    fill_nine(buf, 0xFF, 0x01);
    ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_OK);
    ASSERT_TRUE(val == UINT64_MAX);
    ASSERT_EQ(len, 10u);

    fill_nine(buf, 0x80, 0x01);
    ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_OK);
    ASSERT_TRUE(val == ((uint64_t)1 << 63));

    fill_nine(buf, 0xFF, 0x00); /* padded form of 2^63 - 1 */
    ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_OK);
    ASSERT_TRUE(val == INT64_MAX);
}

TEST(test_decode_past_64_bits)
{
    static const uint8_t tenth[] = {0x02, 0x03, 0x40, 0x7F, 0xFF};
    uint8_t buf[10];
    uint64_t val = 0x1234;
    size_t len = 77;

    for (size_t i = 0; i < sizeof(tenth); i++) {
        fill_nine(buf, 0xFF, tenth[i]);
        ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_ERR_OVERFLOW);
        fill_nine(buf, 0x80, tenth[i]);
        ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_ERR_OVERFLOW);
    }
    ASSERT_TRUE(val == 0x1234);
    ASSERT_EQ(len, 77u);
}

TEST(test_decode_s64_past_64_bits)
{
    uint8_t buf[10];
    int64_t val = 0x1234;
    size_t len = 77;

    fill_nine(buf, 0x80, 0x02); /* 2^64, which used to decode as 0 */
    ASSERT_EQ(varint_decode_s64(buf, 10, &val, &len), VARINT_ERR_OVERFLOW);
    ASSERT_TRUE(val == 0x1234);
    ASSERT_EQ(len, 77u);
}

/* Bit 63 with more bytes promised: short until the eleventh byte arrives. */
TEST(test_decode_eleventh_byte)
{
    uint8_t buf[11];
    uint64_t val = 0;
    size_t len = 0;

    fill_nine(buf, 0xFF, 0x81);
    buf[10] = 0x00;
    ASSERT_EQ(varint_decode_u64(buf, 10, &val, &len), VARINT_ERR_SHORT);
    ASSERT_EQ(varint_decode_u64(buf, 11, &val, &len), VARINT_ERR_OVERFLOW);
}

TEST_MAIN("varint")
    RUN(test_encode_zero);
    RUN(test_encode_127);
    RUN(test_encode_128);
    RUN(test_encode_300);
    RUN(test_encode_max_u64);
    RUN(test_encode_overflow);
    RUN(test_encode_null);
    RUN(test_decode_zero);
    RUN(test_decode_300);
    RUN(test_decode_short);
    RUN(test_decode_null);
    RUN(test_roundtrip_u64);
    RUN(test_zigzag_roundtrip);
    RUN(test_zigzag_zero);
    RUN(test_zigzag_minus1);
    RUN(test_decode_s64_null);
    RUN(test_encoded_size);
    RUN(test_error_codes);
    RUN(test_decode_tenth_byte_top_values);
    RUN(test_decode_past_64_bits);
    RUN(test_decode_s64_past_64_bits);
    RUN(test_decode_eleventh_byte);
TEST_MAIN_END()
