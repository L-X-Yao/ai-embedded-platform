// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "s_record.h"
#include <string.h>

/* ---------- decode S1 (16-bit address) ---------- */

TEST(test_decode_s1)
{
    /* S1130000285F245F2212226A000424290008237C2A */
    const char *line = "S1130000285F245F2212226A000424290008237C2A";
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(line, strlen(line), &rec), S_RECORD_OK);
    ASSERT_EQ((int)rec.type, 1);
    ASSERT_EQ(rec.address, 0x0000u);
    ASSERT_EQ(rec.data_len, 16);
    ASSERT_EQ(rec.data[0], 0x28);
}

TEST(test_decode_s0)
{
    const char *line = "S0030000FC";
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(line, strlen(line), &rec), S_RECORD_OK);
    ASSERT_EQ((int)rec.type, 0);
    ASSERT_EQ(rec.address, 0x0000u);
    ASSERT_EQ(rec.data_len, 0);
}

TEST(test_decode_s9)
{
    const char *line = "S9030000FC";
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(line, strlen(line), &rec), S_RECORD_OK);
    ASSERT_EQ((int)rec.type, 9);
}

TEST(test_decode_s3_32bit_addr)
{
    /* S3: 4-byte address */
    s_record_line_t src = { .type = S_RECORD_S3, .address = 0x08000100, .data_len = 2 };
    src.data[0] = 0xAA;
    src.data[1] = 0xBB;
    char buf[128];
    size_t len = 0;
    s_record_encode(&src, buf, sizeof(buf), &len);

    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(buf, len, &rec), S_RECORD_OK);
    ASSERT_EQ((int)rec.type, 3);
    ASSERT_EQ(rec.address, 0x08000100u);
    ASSERT_EQ(rec.data[0], 0xAA);
}

/* ---------- decode errors ---------- */

TEST(test_decode_null)
{
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(NULL, 10, &rec), S_RECORD_ERR_PARAM);
    ASSERT_EQ(s_record_decode("S0030000FC", 10, NULL), S_RECORD_ERR_PARAM);
}

TEST(test_decode_short)
{
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode("S1", 2, &rec), S_RECORD_ERR_SHORT);
}

TEST(test_decode_no_s)
{
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode("X0030000FC", 10, &rec), S_RECORD_ERR_FORMAT);
}

TEST(test_decode_bad_checksum)
{
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode("S0030000FF", 10, &rec), S_RECORD_ERR_CHECKSUM);
}

TEST(test_decode_with_crlf)
{
    const char *line = "S0030000FC\r\n";
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode(line, strlen(line), &rec), S_RECORD_OK);
}

/* ---------- encode ---------- */

TEST(test_encode_s1)
{
    s_record_line_t rec = { .type = S_RECORD_S1, .address = 0x1000, .data_len = 3 };
    rec.data[0] = 0x01;
    rec.data[1] = 0x02;
    rec.data[2] = 0x03;
    char buf[128];
    size_t len = 0;
    ASSERT_EQ(s_record_encode(&rec, buf, sizeof(buf), &len), S_RECORD_OK);
    ASSERT_EQ(buf[0], 'S');
    ASSERT_EQ(buf[1], '1');
    ASSERT_EQ(buf[len - 2], '\r');
    ASSERT_EQ(buf[len - 1], '\n');
}

TEST(test_encode_null)
{
    char buf[64];
    size_t len;
    s_record_line_t rec = {0};
    ASSERT_EQ(s_record_encode(NULL, buf, 64, &len), S_RECORD_ERR_PARAM);
    ASSERT_EQ(s_record_encode(&rec, NULL, 64, &len), S_RECORD_ERR_PARAM);
    ASSERT_EQ(s_record_encode(&rec, buf, 64, NULL), S_RECORD_ERR_PARAM);
}

TEST(test_encode_overflow)
{
    s_record_line_t rec = { .type = S_RECORD_S1, .data_len = 10 };
    char buf[4];
    size_t len;
    ASSERT_EQ(s_record_encode(&rec, buf, sizeof(buf), &len), S_RECORD_ERR_OVERFLOW);
}

TEST(test_encode_address_too_wide)
{
    /* The record type fixes the address field at 2, 3 or 4 bytes. */
    static const struct { s_record_type_t type; uint32_t address; } cases[] = {
        { S_RECORD_S0, 0x00010000u }, { S_RECORD_S1, 0x00012345u },
        { S_RECORD_S1, 0x00010000u }, { S_RECORD_S5, 0x00010000u },
        { S_RECORD_S9, 0x00010000u }, { S_RECORD_S2, 0x01234567u },
        { S_RECORD_S2, 0x01000000u }, { S_RECORD_S8, 0x01000000u },
    };
    char buf[32];
    size_t len;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        s_record_line_t rec = { .type = cases[i].type, .address = cases[i].address,
                                .data_len = 1, .data = { 0xAA } };
        ASSERT_EQ(s_record_encode(&rec, buf, sizeof(buf), &len), S_RECORD_ERR_OVERFLOW);
    }
}

TEST(test_encode_largest_address_of_each_type)
{
    static const struct { s_record_type_t type; uint32_t address; } cases[] = {
        { S_RECORD_S1, 0x0000FFFFu }, { S_RECORD_S2, 0x00FFFFFFu },
        { S_RECORD_S3, 0xFFFFFFFFu }, { S_RECORD_S7, 0xFFFFFFFFu },
        { S_RECORD_S8, 0x00FFFFFFu }, { S_RECORD_S9, 0x0000FFFFu },
    };
    char buf[32];
    size_t len;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        s_record_line_t rec = { .type = cases[i].type, .address = cases[i].address };
        s_record_line_t back;
        ASSERT_EQ(s_record_encode(&rec, buf, sizeof(buf), &len), S_RECORD_OK);
        ASSERT_EQ(s_record_decode(buf, len, &back), S_RECORD_OK);
        ASSERT_EQ(back.address, cases[i].address);
    }
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip_s1)
{
    s_record_line_t src = { .type = S_RECORD_S1, .address = 0xABCD, .data_len = 4 };
    src.data[0] = 0xDE; src.data[1] = 0xAD; src.data[2] = 0xBE; src.data[3] = 0xEF;
    char buf[128];
    size_t len = 0;
    s_record_encode(&src, buf, sizeof(buf), &len);
    s_record_line_t dst;
    ASSERT_EQ(s_record_decode(buf, len, &dst), S_RECORD_OK);
    ASSERT_EQ(dst.address, 0xABCDu);
    ASSERT_EQ(dst.data_len, 4);
    ASSERT_TRUE(memcmp(dst.data, src.data, 4) == 0);
}

TEST(test_roundtrip_s2)
{
    s_record_line_t src = { .type = S_RECORD_S2, .address = 0x123456, .data_len = 2 };
    src.data[0] = 0x01; src.data[1] = 0x02;
    char buf[128];
    size_t len = 0;
    s_record_encode(&src, buf, sizeof(buf), &len);
    s_record_line_t dst;
    ASSERT_EQ(s_record_decode(buf, len, &dst), S_RECORD_OK);
    ASSERT_EQ(dst.address, 0x123456u);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)S_RECORD_OK, 0);
    ASSERT_EQ((int)S_RECORD_ERR_PARAM, -1);
    ASSERT_EQ((int)S_RECORD_ERR_FORMAT, -2);
    ASSERT_EQ((int)S_RECORD_ERR_SHORT, -3);
    ASSERT_EQ((int)S_RECORD_ERR_OVERFLOW, -4);
    ASSERT_EQ((int)S_RECORD_ERR_CHECKSUM, -5);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    s_record_line_t rec;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        char buf[80];
        int len = (int)(seed % 41) + 4;
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (char)(32 + (seed >> 16) % 95);
        }
        seed = seed * 1103515245u + 12345u;
        s_record_decode(buf, (size_t)len, &rec);
    }
}

TEST(test_decode_count_zero)
{
    s_record_line_t rec;
    ASSERT_EQ(s_record_decode("S100", 4, &rec), S_RECORD_ERR_FORMAT);
}

TEST(test_decode_count_too_small_for_address)
{
    s_record_line_t rec;

    /* S1 needs two address bytes plus the checksum. */
    ASSERT_EQ(s_record_decode("S101FE", 6, &rec), S_RECORD_ERR_FORMAT);

    /* S3 needs four address bytes plus the checksum. */
    ASSERT_EQ(s_record_decode("S304000000FB", 12, &rec), S_RECORD_ERR_FORMAT);
}

TEST_MAIN("s_record")
    RUN(test_decode_s1);
    RUN(test_decode_s0);
    RUN(test_decode_s9);
    RUN(test_decode_s3_32bit_addr);
    RUN(test_decode_null);
    RUN(test_decode_short);
    RUN(test_decode_no_s);
    RUN(test_decode_bad_checksum);
    RUN(test_decode_with_crlf);
    RUN(test_encode_s1);
    RUN(test_encode_null);
    RUN(test_encode_overflow);
    RUN(test_encode_address_too_wide);
    RUN(test_encode_largest_address_of_each_type);
    RUN(test_roundtrip_s1);
    RUN(test_roundtrip_s2);
    RUN(test_error_codes);
    RUN(test_fuzz);
    RUN(test_decode_count_zero);
    RUN(test_decode_count_too_small_for_address);
TEST_MAIN_END()
