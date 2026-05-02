// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "hex_codec.h"

/* ---- encode ---------------------------------------------------- */

TEST(test_encode_empty)
{
    char buf[4] = {0};
    ASSERT_EQ(hex_encode(NULL, 0, buf, sizeof(buf)), HEX_OK);
    ASSERT_EQ(buf[0], '\0');
}

TEST(test_encode_single_byte)
{
    char buf[4];
    const uint8_t data[] = {0xAB};
    ASSERT_EQ(hex_encode(data, 1, buf, sizeof(buf)), HEX_OK);
    ASSERT_TRUE(strcmp(buf, "ab") == 0);
}

TEST(test_encode_multiple_bytes)
{
    char buf[16];
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(hex_encode(data, 4, buf, sizeof(buf)), HEX_OK);
    ASSERT_TRUE(strcmp(buf, "deadbeef") == 0);
}

TEST(test_encode_uppercase)
{
    char buf[16];
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(hex_encode_upper(data, 4, buf, sizeof(buf)), HEX_OK);
    ASSERT_TRUE(strcmp(buf, "DEADBEEF") == 0);
}

/* ---- decode ---------------------------------------------------- */

TEST(test_decode_roundtrip)
{
    const uint8_t orig[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    char hex_buf[20];
    ASSERT_EQ(hex_encode(orig, 8, hex_buf, sizeof(hex_buf)), HEX_OK);

    uint8_t decoded[8];
    uint16_t decoded_len = sizeof(decoded);
    ASSERT_EQ(hex_decode(hex_buf, 16, decoded, &decoded_len), HEX_OK);
    ASSERT_EQ(decoded_len, 8);
    ASSERT_TRUE(memcmp(orig, decoded, 8) == 0);
}

TEST(test_decode_mixed_case)
{
    const char mixed[] = "aAbBcCdD";
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(hex_decode(mixed, 8, out, &out_len), HEX_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ_HEX(out[0], 0xAA);
    ASSERT_EQ_HEX(out[1], 0xBB);
    ASSERT_EQ_HEX(out[2], 0xCC);
    ASSERT_EQ_HEX(out[3], 0xDD);
}

TEST(test_decode_odd_length)
{
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(hex_decode("abc", 3, out, &out_len), HEX_ERR_ODD_LENGTH);
}

TEST(test_decode_invalid_char)
{
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(hex_decode("zz", 2, out, &out_len), HEX_ERR_INVALID);
    out_len = sizeof(out);
    ASSERT_EQ(hex_decode("0g", 2, out, &out_len), HEX_ERR_INVALID);
}

/* ---- error paths ----------------------------------------------- */

TEST(test_null_params)
{
    char cbuf[4];
    uint8_t bbuf[4];
    uint16_t len = sizeof(bbuf);

    /* encode: NULL data with len > 0 */
    ASSERT_EQ(hex_encode(NULL, 1, cbuf, sizeof(cbuf)), HEX_ERR_PARAM);
    /* encode: NULL out */
    const uint8_t d = 0;
    ASSERT_EQ(hex_encode(&d, 1, NULL, 4), HEX_ERR_PARAM);
    /* decode: NULL out_len */
    ASSERT_EQ(hex_decode("aa", 2, bbuf, NULL), HEX_ERR_PARAM);
    /* decode: NULL hex with len > 0 */
    ASSERT_EQ(hex_decode(NULL, 2, bbuf, &len), HEX_ERR_PARAM);
    /* decode: NULL out with len > 0 */
    len = 4;
    ASSERT_EQ(hex_decode("aa", 2, NULL, &len), HEX_ERR_PARAM);
}

TEST(test_buffer_overflow)
{
    char cbuf[2]; /* too small for 1 byte (needs 3) */
    const uint8_t d = 0xFF;
    ASSERT_EQ(hex_encode(&d, 1, cbuf, sizeof(cbuf)), HEX_ERR_OVERFLOW);

    uint8_t bbuf[1];
    uint16_t len = 1;
    ASSERT_EQ(hex_decode("aabb", 4, bbuf, &len), HEX_ERR_OVERFLOW);
}

/* ---- size calculations ----------------------------------------- */

TEST(test_size_calculations)
{
    ASSERT_EQ(hex_encoded_size(0), 0);
    ASSERT_EQ(hex_encoded_size(1), 2);
    ASSERT_EQ(hex_encoded_size(16), 32);

    ASSERT_EQ(hex_decoded_size(0), 0);
    ASSERT_EQ(hex_decoded_size(2), 1);
    ASSERT_EQ(hex_decoded_size(32), 16);
    /* odd input: integer division truncates */
    ASSERT_EQ(hex_decoded_size(3), 1);
}

/* ---- nibble helpers -------------------------------------------- */

TEST(test_nibble_to_char)
{
    ASSERT_EQ(hex_nibble_to_char(0, false), '0');
    ASSERT_EQ(hex_nibble_to_char(9, false), '9');
    ASSERT_EQ(hex_nibble_to_char(10, false), 'a');
    ASSERT_EQ(hex_nibble_to_char(15, false), 'f');
    ASSERT_EQ(hex_nibble_to_char(10, true), 'A');
    ASSERT_EQ(hex_nibble_to_char(15, true), 'F');
    /* values > 15 are masked */
    ASSERT_EQ(hex_nibble_to_char(0x1A, false), 'a');
}

TEST(test_char_to_nibble)
{
    ASSERT_EQ(hex_char_to_nibble('0'), 0);
    ASSERT_EQ(hex_char_to_nibble('9'), 9);
    ASSERT_EQ(hex_char_to_nibble('a'), 10);
    ASSERT_EQ(hex_char_to_nibble('f'), 15);
    ASSERT_EQ(hex_char_to_nibble('A'), 10);
    ASSERT_EQ(hex_char_to_nibble('F'), 15);
    ASSERT_EQ(hex_char_to_nibble('g'), -1);
    ASSERT_EQ(hex_char_to_nibble(' '), -1);
    ASSERT_EQ(hex_char_to_nibble('\0'), -1);
}

/* ---- full roundtrip 0x00-0xFF ---------------------------------- */

TEST(test_full_byte_range_roundtrip)
{
    uint8_t all_bytes[256];
    for (int i = 0; i < 256; i++) {
        all_bytes[i] = (uint8_t)i;
    }

    char hex_buf[513]; /* 256*2 + 1 */
    ASSERT_EQ(hex_encode(all_bytes, 256, hex_buf, sizeof(hex_buf)), HEX_OK);
    ASSERT_EQ(strlen(hex_buf), 512u);

    /* verify first few and last few chars */
    ASSERT_TRUE(hex_buf[0] == '0' && hex_buf[1] == '0'); /* 0x00 */
    ASSERT_TRUE(hex_buf[2] == '0' && hex_buf[3] == '1'); /* 0x01 */
    ASSERT_TRUE(hex_buf[508] == 'f' && hex_buf[509] == 'e'); /* 0xFE */
    ASSERT_TRUE(hex_buf[510] == 'f' && hex_buf[511] == 'f'); /* 0xFF */

    /* decode back */
    uint8_t decoded[256];
    uint16_t decoded_len = sizeof(decoded);
    ASSERT_EQ(hex_decode(hex_buf, 512, decoded, &decoded_len), HEX_OK);
    ASSERT_EQ(decoded_len, 256);
    ASSERT_TRUE(memcmp(all_bytes, decoded, 256) == 0);
}

/* ---- leading zeros --------------------------------------------- */

TEST(test_decode_leading_zeros)
{
    const char hex[] = "00000102";
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(hex_decode(hex, 8, out, &out_len), HEX_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(out[0], 0x00);
    ASSERT_EQ(out[1], 0x00);
    ASSERT_EQ(out[2], 0x01);
    ASSERT_EQ(out[3], 0x02);
}

/* ---- decode empty ---------------------------------------------- */

TEST(test_decode_empty)
{
    uint8_t out[4];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(hex_decode("", 0, out, &out_len), HEX_OK);
    ASSERT_EQ(out_len, 0);
}

TEST_MAIN("hex_codec")
    RUN(test_encode_empty);
    RUN(test_encode_single_byte);
    RUN(test_encode_multiple_bytes);
    RUN(test_encode_uppercase);
    RUN(test_decode_roundtrip);
    RUN(test_decode_mixed_case);
    RUN(test_decode_odd_length);
    RUN(test_decode_invalid_char);
    RUN(test_null_params);
    RUN(test_buffer_overflow);
    RUN(test_size_calculations);
    RUN(test_nibble_to_char);
    RUN(test_char_to_nibble);
    RUN(test_full_byte_range_roundtrip);
    RUN(test_decode_leading_zeros);
    RUN(test_decode_empty);
TEST_MAIN_END()
