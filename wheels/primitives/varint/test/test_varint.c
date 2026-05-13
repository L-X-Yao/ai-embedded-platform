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
TEST_MAIN_END()
