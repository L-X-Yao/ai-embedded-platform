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

TEST(test_decode_rejects_undefined_record_type)
{
    intel_hex_record_t rec;
    /* the checksums are right, so only the type check can refuse these */
    ASSERT_EQ(intel_hex_decode(":00000006FA", 11, &rec), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(intel_hex_decode(":000000FF01", 11, &rec), INTEL_HEX_ERR_FORMAT);
}

TEST(test_decode_rejects_text_after_checksum)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(":00000001FFjunk", 15, &rec), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(intel_hex_decode(":00000001FF00", 13, &rec), INTEL_HEX_ERR_FORMAT);
}

TEST(test_decode_allows_line_ending_after_checksum)
{
    intel_hex_record_t rec;
    ASSERT_EQ(intel_hex_decode(":00000001FF\r\n", 13, &rec), INTEL_HEX_OK);
    ASSERT_EQ(intel_hex_decode(":00000001FF\n", 12, &rec), INTEL_HEX_OK);
}

TEST(test_encode_rejects_undefined_record_type)
{
    intel_hex_record_t rec;
    char buf[32];
    size_t len = 0;

    memset(&rec, 0, sizeof(rec));
    rec.type = (intel_hex_type_t)(INTEL_HEX_START_LINEAR + 1);
    ASSERT_EQ(intel_hex_encode(&rec, buf, sizeof(buf), &len), INTEL_HEX_ERR_PARAM);
}

/* ---------- the shape of each record type ---------- */

static intel_hex_err_t decode_str(const char *line)
{
    intel_hex_record_t rec;
    return intel_hex_decode(line, strlen(line), &rec);
}

TEST(test_decode_rejects_wrong_byte_count_for_type)
{
    /* Checksums worked out by hand: two's complement of the byte sum. */
    ASSERT_EQ(decode_str(":02000001AABB98"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":0100000255A8"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":020000031234B5"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":01000004AA51"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":020000050001F8"), INTEL_HEX_ERR_FORMAT);
}

TEST(test_decode_rejects_address_on_address_records)
{
    ASSERT_EQ(decode_str(":020010021234A6"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":0400100300000000E9"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":02123404FFFFB4"), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(decode_str(":0400100500000000E7"), INTEL_HEX_ERR_FORMAT);
}

TEST(test_decode_accepts_each_valid_shape)
{
    intel_hex_record_t rec;
    const char *ext_seg = ":020000021000EC";
    const char *start_seg = ":0400000300003800C1";
    const char *ext_lin = ":020000040800F2";
    const char *start_lin = ":04000005000000CD2A";

    ASSERT_EQ(intel_hex_decode(ext_seg, strlen(ext_seg), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.data_len, 2);
    ASSERT_EQ(intel_hex_decode(start_seg, strlen(start_seg), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.data_len, 4);
    ASSERT_EQ(intel_hex_decode(ext_lin, strlen(ext_lin), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.data_len, 2);
    ASSERT_EQ(intel_hex_decode(start_lin, strlen(start_lin), &rec), INTEL_HEX_OK);
    ASSERT_EQ((int)rec.data_len, 4);
}

TEST(test_decode_accepts_eof_with_address)
{
    /* Some tools put the start address here, and it carries no data. */
    ASSERT_EQ(decode_str(":00123401B9"), INTEL_HEX_OK);
}

static intel_hex_err_t encode_shape(intel_hex_type_t type, uint16_t address,
                                    uint8_t data_len)
{
    intel_hex_record_t rec;
    char buf[80];
    size_t len = 0;

    memset(&rec, 0, sizeof(rec));
    rec.type = type;
    rec.address = address;
    rec.data_len = data_len;
    return intel_hex_encode(&rec, buf, sizeof(buf), &len);
}

TEST(test_encode_rejects_wrong_shape)
{
    ASSERT_EQ(encode_shape(INTEL_HEX_EOF, 0, 1), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_SEGMENT, 0, 1), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_SEGMENT, 0, 2), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_LINEAR, 0, 4), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_LINEAR, 0, 0), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_SEGMENT, 1, 2), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_SEGMENT, 1, 4), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_LINEAR, 0x8000, 2), INTEL_HEX_ERR_FORMAT);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_LINEAR, 2, 4), INTEL_HEX_ERR_FORMAT);
}

TEST(test_encode_accepts_each_valid_shape)
{
    ASSERT_EQ(encode_shape(INTEL_HEX_DATA, 0x1234, 3), INTEL_HEX_OK);
    ASSERT_EQ(encode_shape(INTEL_HEX_EOF, 0x1234, 0), INTEL_HEX_OK);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_SEGMENT, 0, 2), INTEL_HEX_OK);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_SEGMENT, 0, 4), INTEL_HEX_OK);
    ASSERT_EQ(encode_shape(INTEL_HEX_EXT_LINEAR, 0, 2), INTEL_HEX_OK);
    ASSERT_EQ(encode_shape(INTEL_HEX_START_LINEAR, 0, 4), INTEL_HEX_OK);
}

TEST_MAIN("intel_hex")
    RUN(test_decode_rejects_wrong_byte_count_for_type);
    RUN(test_decode_rejects_address_on_address_records);
    RUN(test_decode_accepts_each_valid_shape);
    RUN(test_decode_accepts_eof_with_address);
    RUN(test_encode_rejects_wrong_shape);
    RUN(test_encode_accepts_each_valid_shape);
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
    RUN(test_decode_rejects_undefined_record_type);
    RUN(test_decode_rejects_text_after_checksum);
    RUN(test_decode_allows_line_ending_after_checksum);
    RUN(test_encode_rejects_undefined_record_type);
TEST_MAIN_END()
