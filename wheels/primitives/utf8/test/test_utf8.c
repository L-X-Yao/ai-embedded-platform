// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "utf8.h"
#include <string.h>

/* ---------- byte_length ---------- */

TEST(test_byte_length)
{
    ASSERT_EQ(utf8_byte_length(0x00), 1);
    ASSERT_EQ(utf8_byte_length(0x7F), 1);
    ASSERT_EQ(utf8_byte_length(0xC2), 2);
    ASSERT_EQ(utf8_byte_length(0xE0), 3);
    ASSERT_EQ(utf8_byte_length(0xF0), 4);
    ASSERT_EQ(utf8_byte_length(0x80), 0);
    ASSERT_EQ(utf8_byte_length(0xFF), 0);
}

/* ---------- decode ---------- */

TEST(test_decode_ascii)
{
    uint8_t data[] = {'A'};
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 1, &cp, &n), UTF8_OK);
    ASSERT_EQ(cp, 0x41u);
    ASSERT_EQ(n, 1u);
}

TEST(test_decode_2byte)
{
    uint8_t data[] = {0xC3, 0xA9};  /* é = U+00E9 */
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 2, &cp, &n), UTF8_OK);
    ASSERT_EQ(cp, 0xE9u);
    ASSERT_EQ(n, 2u);
}

TEST(test_decode_3byte)
{
    uint8_t data[] = {0xE4, 0xB8, 0xAD};  /* 中 = U+4E2D */
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 3, &cp, &n), UTF8_OK);
    ASSERT_EQ(cp, 0x4E2Du);
}

TEST(test_decode_4byte)
{
    uint8_t data[] = {0xF0, 0x9F, 0x98, 0x80};  /* 😀 = U+1F600 */
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 4, &cp, &n), UTF8_OK);
    ASSERT_EQ(cp, 0x1F600u);
}

TEST(test_decode_overlong_rejected)
{
    uint8_t data[] = {0xC0, 0x80};  /* overlong NUL */
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 2, &cp, &n), UTF8_ERR_INVALID);
}

TEST(test_decode_surrogate_rejected)
{
    uint8_t data[] = {0xED, 0xA0, 0x80};  /* U+D800 surrogate */
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 3, &cp, &n), UTF8_ERR_INVALID);
}

TEST(test_decode_short)
{
    uint8_t data[] = {0xC3};
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(data, 1, &cp, &n), UTF8_ERR_SHORT);
}

TEST(test_decode_null)
{
    uint32_t cp; size_t n;
    ASSERT_EQ(utf8_decode(NULL, 1, &cp, &n), UTF8_ERR_PARAM);
}

/* ---------- encode ---------- */

TEST(test_encode_ascii)
{
    uint8_t buf[4]; size_t n;
    ASSERT_EQ(utf8_encode(0x41, buf, 4, &n), UTF8_OK);
    ASSERT_EQ(n, 1u);
    ASSERT_EQ(buf[0], 'A');
}

TEST(test_encode_2byte)
{
    uint8_t buf[4]; size_t n;
    ASSERT_EQ(utf8_encode(0xE9, buf, 4, &n), UTF8_OK);
    ASSERT_EQ(n, 2u);
    ASSERT_EQ(buf[0], 0xC3);
    ASSERT_EQ(buf[1], 0xA9);
}

TEST(test_encode_emoji)
{
    uint8_t buf[4]; size_t n;
    ASSERT_EQ(utf8_encode(0x1F600, buf, 4, &n), UTF8_OK);
    ASSERT_EQ(n, 4u);
}

TEST(test_encode_surrogate_rejected)
{
    uint8_t buf[4]; size_t n;
    ASSERT_EQ(utf8_encode(0xD800, buf, 4, &n), UTF8_ERR_INVALID);
}

TEST(test_encode_too_large)
{
    uint8_t buf[4]; size_t n;
    ASSERT_EQ(utf8_encode(0x110000, buf, 4, &n), UTF8_ERR_INVALID);
}

TEST(test_encode_buf_too_small)
{
    uint8_t buf[1]; size_t n;
    ASSERT_EQ(utf8_encode(0xE9, buf, 1, &n), UTF8_ERR_SHORT);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip)
{
    uint32_t cps[] = {0, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF};
    for (int i = 0; i < (int)(sizeof(cps)/sizeof(cps[0])); i++) {
        if (cps[i] >= 0xD800 && cps[i] <= 0xDFFF) continue;
        uint8_t buf[4]; size_t elen, dlen;
        uint32_t decoded;
        utf8_encode(cps[i], buf, 4, &elen);
        utf8_decode(buf, elen, &decoded, &dlen);
        ASSERT_EQ(decoded, cps[i]);
    }
}

/* ---------- validate ---------- */

TEST(test_validate_ok)
{
    const uint8_t *s = (const uint8_t *)"Hello";
    ASSERT_EQ(utf8_validate(s, 5), UTF8_OK);
}

TEST(test_validate_mixed)
{
    uint8_t s[] = {0xE4, 0xB8, 0xAD, 'A', 0xF0, 0x9F, 0x98, 0x80};
    ASSERT_EQ(utf8_validate(s, sizeof(s)), UTF8_OK);
}

TEST(test_validate_bad)
{
    uint8_t s[] = {0xFF};
    ASSERT_EQ(utf8_validate(s, 1), UTF8_ERR_INVALID);
}

/* ---------- codepoint_count ---------- */

TEST(test_count_ascii)
{
    ASSERT_EQ(utf8_codepoint_count((const uint8_t *)"abc", 3), 3);
}

TEST(test_count_chinese)
{
    uint8_t s[] = {0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87};  /* 中文 */
    ASSERT_EQ(utf8_codepoint_count(s, sizeof(s)), 2);
}

TEST(test_count_invalid)
{
    uint8_t s[] = {0xFF};
    ASSERT_EQ(utf8_codepoint_count(s, 1), -1);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)UTF8_OK, 0);
    ASSERT_EQ((int)UTF8_ERR_PARAM, -1);
    ASSERT_EQ((int)UTF8_ERR_INVALID, -2);
    ASSERT_EQ((int)UTF8_ERR_SHORT, -3);
}

TEST_MAIN("utf8")
    RUN(test_byte_length);
    RUN(test_decode_ascii);
    RUN(test_decode_2byte);
    RUN(test_decode_3byte);
    RUN(test_decode_4byte);
    RUN(test_decode_overlong_rejected);
    RUN(test_decode_surrogate_rejected);
    RUN(test_decode_short);
    RUN(test_decode_null);
    RUN(test_encode_ascii);
    RUN(test_encode_2byte);
    RUN(test_encode_emoji);
    RUN(test_encode_surrogate_rejected);
    RUN(test_encode_too_large);
    RUN(test_encode_buf_too_small);
    RUN(test_roundtrip);
    RUN(test_validate_ok);
    RUN(test_validate_mixed);
    RUN(test_validate_bad);
    RUN(test_count_ascii);
    RUN(test_count_chinese);
    RUN(test_count_invalid);
    RUN(test_error_codes);
TEST_MAIN_END()
