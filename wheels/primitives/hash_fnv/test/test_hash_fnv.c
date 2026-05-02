// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "hash_fnv.h"

/*
 * Test vectors from http://www.isthe.com/chongo/tech/comp/fnv/
 * FNV-1a variant.
 */

/* --- 32-bit one-shot tests --- */

TEST(test_32_empty)
{
    /* Empty input returns offset basis */
    ASSERT_EQ_HEX(fnv1a_32(NULL, 0), 0x811C9DC5u);
    ASSERT_EQ_HEX(fnv1a_32((const uint8_t *)"", 0), 0x811C9DC5u);
}

TEST(test_32_known_vectors)
{
    /* Well-known FNV-1a 32-bit test vectors */
    ASSERT_EQ_HEX(fnv1a_32_string(""),       0x811C9DC5u);
    ASSERT_EQ_HEX(fnv1a_32_string("a"),      0xE40C292Cu);
    ASSERT_EQ_HEX(fnv1a_32_string("foobar"), 0xBF9CF968u);
}

TEST(test_32_single_bytes)
{
    /* Single byte 0x00 */
    uint8_t z = 0x00;
    ASSERT_EQ_HEX(fnv1a_32(&z, 1), 0x050C5D1Fu);
    /* Single byte 0xFF: (0x811C9DC5 ^ 0xFF) * 0x01000193 */
    uint8_t f = 0xFF;
    ASSERT_EQ_HEX(fnv1a_32(&f, 1), 0x7A0B824Eu);
}

TEST(test_32_string_vs_buffer)
{
    /* fnv1a_32_string("abc") == fnv1a_32((uint8_t*)"abc", 3) */
    const char *s = "abc";
    ASSERT_EQ_HEX(fnv1a_32_string(s),
                   fnv1a_32((const uint8_t *)s, strlen(s)));
}

TEST(test_32_null_string)
{
    /* NULL string returns offset basis */
    ASSERT_EQ_HEX(fnv1a_32_string(NULL), 0x811C9DC5u);
}

/* --- 64-bit one-shot tests --- */

TEST(test_64_empty)
{
    ASSERT_EQ_HEX(fnv1a_64(NULL, 0), 0xCBF29CE484222325ULL);
    ASSERT_EQ_HEX(fnv1a_64((const uint8_t *)"", 0), 0xCBF29CE484222325ULL);
}

TEST(test_64_known_vectors)
{
    ASSERT_EQ_HEX(fnv1a_64_string(""),       0xCBF29CE484222325ULL);
    ASSERT_EQ_HEX(fnv1a_64_string("a"),      0xAF63DC4C8601EC8CULL);
    ASSERT_EQ_HEX(fnv1a_64_string("foobar"), 0x85944171F73967E8ULL);
}

TEST(test_64_string_vs_buffer)
{
    const char *s = "abc";
    ASSERT_EQ_HEX(fnv1a_64_string(s),
                   fnv1a_64((const uint8_t *)s, strlen(s)));
}

TEST(test_64_null_string)
{
    ASSERT_EQ_HEX(fnv1a_64_string(NULL), 0xCBF29CE484222325ULL);
}

/* --- Incremental (update) tests --- */

TEST(test_32_update_streaming)
{
    /* Streaming "foobar" as "foo" + "bar" must equal one-shot */
    uint32_t h = FNV1A_32_OFFSET_BASIS;
    h = fnv1a_32_update(h, (const uint8_t *)"foo", 3);
    h = fnv1a_32_update(h, (const uint8_t *)"bar", 3);
    ASSERT_EQ_HEX(h, fnv1a_32_string("foobar"));
}

TEST(test_64_update_streaming)
{
    uint64_t h = FNV1A_64_OFFSET_BASIS;
    h = fnv1a_64_update(h, (const uint8_t *)"foo", 3);
    h = fnv1a_64_update(h, (const uint8_t *)"bar", 3);
    ASSERT_EQ_HEX(h, fnv1a_64_string("foobar"));
}

TEST(test_32_update_byte_by_byte)
{
    /* Byte-by-byte streaming equals one-shot */
    const char *s = "hello";
    uint32_t h = FNV1A_32_OFFSET_BASIS;
    for (size_t i = 0; i < strlen(s); i++)
        h = fnv1a_32_update(h, (const uint8_t *)&s[i], 1);
    ASSERT_EQ_HEX(h, fnv1a_32_string("hello"));
}

TEST(test_64_update_byte_by_byte)
{
    const char *s = "hello";
    uint64_t h = FNV1A_64_OFFSET_BASIS;
    for (size_t i = 0; i < strlen(s); i++)
        h = fnv1a_64_update(h, (const uint8_t *)&s[i], 1);
    ASSERT_EQ_HEX(h, fnv1a_64_string("hello"));
}

/* --- Collision resistance (different inputs differ) --- */

TEST(test_32_different_strings_differ)
{
    uint32_t h1 = fnv1a_32_string("foo");
    uint32_t h2 = fnv1a_32_string("bar");
    uint32_t h3 = fnv1a_32_string("baz");
    ASSERT_TRUE(h1 != h2);
    ASSERT_TRUE(h2 != h3);
    ASSERT_TRUE(h1 != h3);
}

/* --- Update with NULL data is no-op --- */

TEST(test_update_null_noop)
{
    uint32_t h32 = FNV1A_32_OFFSET_BASIS;
    ASSERT_EQ_HEX(fnv1a_32_update(h32, NULL, 5), FNV1A_32_OFFSET_BASIS);
    uint64_t h64 = FNV1A_64_OFFSET_BASIS;
    ASSERT_EQ_HEX(fnv1a_64_update(h64, NULL, 5), FNV1A_64_OFFSET_BASIS);
}

TEST_MAIN("hash_fnv")
    RUN(test_32_empty);
    RUN(test_32_known_vectors);
    RUN(test_32_single_bytes);
    RUN(test_32_string_vs_buffer);
    RUN(test_32_null_string);
    RUN(test_64_empty);
    RUN(test_64_known_vectors);
    RUN(test_64_string_vs_buffer);
    RUN(test_64_null_string);
    RUN(test_32_update_streaming);
    RUN(test_64_update_streaming);
    RUN(test_32_update_byte_by_byte);
    RUN(test_64_update_byte_by_byte);
    RUN(test_32_different_strings_differ);
    RUN(test_update_null_noop);
TEST_MAIN_END()
