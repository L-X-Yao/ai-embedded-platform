// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "intel_hex.h"
#include <string.h>

/* ---------- decode data record ---------- */

TEST(test_decode_data)
{
    /* :10010000214601360121470136007EFE09D2190140 */
    const char *line = ":10010000214601360121470136007EFE09D2190140";
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(line, strlen(line), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.type, INTEL_HEX_DATA);
    ASSERT_EQ(rec.address, 0x0100u);
    ASSERT_EQ(rec.data_len, 16);
    ASSERT_EQ(rec.data[0], 0x21);
}

TEST(test_decode_eof)
{
    const char *line = ":00000001FF";
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(line, strlen(line), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.type, INTEL_HEX_EOF);
    ASSERT_EQ(rec.data_len, 0);
}

TEST(test_decode_ext_linear)
{
    /* :02000004FFFFFC */
    const char *line = ":02000004FFFFFC";
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(line, strlen(line), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.type, INTEL_HEX_EXT_LINEAR);
    ASSERT_EQ(rec.data_len, 2);
    ASSERT_EQ(rec.data[0], 0xFF);
    ASSERT_EQ(rec.data[1], 0xFF);
}

TEST(test_decode_with_crlf)
{
    const char *line = ":00000001FF\r\n";
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(line, strlen(line), &rec), INTEL_HEX_OK);
}

/* ---------- decode errors ---------- */

TEST(test_decode_null)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(NULL, 10, &rec), INTEL_HEX_ERR_PARAM);
    ASSERT_EQ(intel_hex_decode(":00000001FF", 11, NULL), INTEL_HEX_ERR_PARAM);
}

TEST(test_decode_short)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(":0000", 5, &rec), INTEL_HEX_ERR_SHORT);
}

TEST(test_decode_no_colon)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode("X00000001FF", 11, &rec), INTEL_HEX_ERR_FORMAT);
}

TEST(test_decode_bad_checksum)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(":00000001FE", 11, &rec), INTEL_HEX_ERR_CHECKSUM);
}

TEST(test_decode_bad_hex)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(":0000XX01FF", 11, &rec), INTEL_HEX_ERR_FORMAT);
}

/* ---------- encode ---------- */

TEST(test_encode_eof)
{
    intel_hex_record_t rec = { .type = INTEL_HEX_EOF, .address = 0, .data_len = 0 };
    char buf[64];
    size_t len = 0;
    ASSERT_EQ(intel_hex_encode(&rec, buf, sizeof(buf), &len), INTEL_HEX_OK);
    ASSERT_EQ(buf[0], ':');
    ASSERT_EQ(buf[len - 2], '\r');
    ASSERT_EQ(buf[len - 1], '\n');
}

TEST(test_encode_null)
{
    char buf[64];
    size_t len;
    intel_hex_record_t rec = {0};
    ASSERT_EQ(intel_hex_encode(NULL, buf, 64, &len), INTEL_HEX_ERR_PARAM);
    ASSERT_EQ(intel_hex_encode(&rec, NULL, 64, &len), INTEL_HEX_ERR_PARAM);
    ASSERT_EQ(intel_hex_encode(&rec, buf, 64, NULL), INTEL_HEX_ERR_PARAM);
}

TEST(test_encode_overflow)
{
    intel_hex_record_t rec = { .type = INTEL_HEX_DATA, .data_len = 16 };
    char buf[4];
    size_t len;
    ASSERT_EQ(intel_hex_encode(&rec, buf, sizeof(buf), &len), INTEL_HEX_ERR_OVERFLOW);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip_data)
{
    intel_hex_record_t src = { .type = INTEL_HEX_DATA, .address = 0x1234, .data_len = 4 };
    src.data[0] = 0xDE; src.data[1] = 0xAD; src.data[2] = 0xBE; src.data[3] = 0xEF;
    char buf[128];
    size_t len = 0;
    intel_hex_encode(&src, buf, sizeof(buf), &len);
    intel_hex_record_t dst;
    ASSERT_EQ(intel_hex_decode(buf, len, &dst), INTEL_HEX_OK);
    ASSERT_EQ(dst.address, 0x1234u);
    ASSERT_EQ(dst.data_len, 4);
    ASSERT_TRUE(memcmp(dst.data, src.data, 4) == 0);
}

TEST(test_roundtrip_eof)
{
    intel_hex_record_t src = { .type = INTEL_HEX_EOF, .address = 0, .data_len = 0 };
    char buf[64];
    size_t len = 0;
    intel_hex_encode(&src, buf, sizeof(buf), &len);
    intel_hex_record_t dst;
    ASSERT_EQ(intel_hex_decode(buf, len, &dst), INTEL_HEX_OK);
    ASSERT_EQ((int)dst.type, INTEL_HEX_EOF);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)INTEL_HEX_OK, 0);
    ASSERT_EQ((int)INTEL_HEX_ERR_PARAM, -1);
    ASSERT_EQ((int)INTEL_HEX_ERR_FORMAT, -2);
    ASSERT_EQ((int)INTEL_HEX_ERR_SHORT, -3);
    ASSERT_EQ((int)INTEL_HEX_ERR_OVERFLOW, -4);
    ASSERT_EQ((int)INTEL_HEX_ERR_CHECKSUM, -5);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    intel_hex_record_t rec;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        char buf[80];
        int len = (int)(seed % 41) + 4;
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (char)(32 + (seed >> 16) % 95);
        }
        seed = seed * 1103515245u + 12345u;
        intel_hex_decode(buf, (size_t)len, &rec);
    }
}

TEST_MAIN("intel_hex")
    RUN(test_decode_data);
    RUN(test_decode_eof);
    RUN(test_decode_ext_linear);
    RUN(test_decode_with_crlf);
    RUN(test_decode_null);
    RUN(test_decode_short);
    RUN(test_decode_no_colon);
    RUN(test_decode_bad_checksum);
    RUN(test_decode_bad_hex);
    RUN(test_encode_eof);
    RUN(test_encode_null);
    RUN(test_encode_overflow);
    RUN(test_roundtrip_data);
    RUN(test_roundtrip_eof);
    RUN(test_error_codes);
    RUN(test_fuzz);
TEST_MAIN_END()
