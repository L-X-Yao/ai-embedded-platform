// SPDX-License-Identifier: MIT
/**
 * @file    test_sha256.c
 * @brief   C unit tests for sha256 wheel (NIST test vectors).
 *
 * Vectors from:
 *   - NIST FIPS 180-4 examples
 *   - NIST CSRC byte-oriented examples
 */

#include "test_harness.h"
#include "sha256.h"
#include <string.h>

/* Helper: compare hash against hex-encoded expected value. */
static int hash_eq(const uint8_t hash[32], const uint8_t expected[32])
{
    return memcmp(hash, expected, 32) == 0;
}

/* ---- NIST vector: "abc" ---- */

TEST(test_abc)
{
    static const uint8_t expected[32] = {
        0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
        0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
        0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
        0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
    };
    uint8_t hash[32];
    sha256_hash((const uint8_t *)"abc", 3, hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- NIST vector: empty string ---- */

TEST(test_empty)
{
    static const uint8_t expected[32] = {
        0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14,
        0x9A, 0xFB, 0xF4, 0xC8, 0x99, 0x6F, 0xB9, 0x24,
        0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B, 0x93, 0x4C,
        0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55,
    };
    uint8_t hash[32];
    sha256_hash(NULL, 0, hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- NIST vector: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *      (448 bits, two blocks after padding) ---- */

TEST(test_448bit)
{
    static const char msg[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static const uint8_t expected[32] = {
        0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
        0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
        0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
        0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1,
    };
    uint8_t hash[32];
    sha256_hash((const uint8_t *)msg, strlen(msg), hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- Long message: 1,000,000 times 'a' ---- */

TEST(test_million_a)
{
    static const uint8_t expected[32] = {
        0xCD, 0xC7, 0x6E, 0x5C, 0x99, 0x14, 0xFB, 0x92,
        0x81, 0xA1, 0xC7, 0xE2, 0x84, 0xD7, 0x3E, 0x67,
        0xF1, 0x80, 0x9A, 0x48, 0xA4, 0x97, 0x20, 0x0E,
        0x04, 0x6D, 0x39, 0xCC, 0xC7, 0x11, 0x2C, 0xD0,
    };

    sha256_t ctx;
    sha256_init(&ctx);

    /* Feed in chunks of 1000 bytes. */
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof(chunk));
    for (int i = 0; i < 1000; i++) {
        sha256_update(&ctx, chunk, sizeof(chunk));
    }

    uint8_t hash[32];
    sha256_final(&ctx, hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- Streaming equivalence: init/update/final == one-shot ---- */

TEST(test_streaming_equivalence)
{
    const uint8_t msg[] = "The quick brown fox jumps over the lazy dog";
    size_t len = sizeof(msg) - 1;

    uint8_t oneshot[32];
    sha256_hash(msg, len, oneshot);

    /* Feed byte by byte. */
    sha256_t ctx;
    sha256_init(&ctx);
    for (size_t i = 0; i < len; i++) {
        sha256_update(&ctx, &msg[i], 1);
    }
    uint8_t streaming[32];
    sha256_final(&ctx, streaming);

    ASSERT_TRUE(hash_eq(oneshot, streaming));
}

/* ---- Chunked streaming: various split points ---- */

TEST(test_chunked_streaming)
{
    const uint8_t msg[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    size_t len = sizeof(msg) - 1;

    uint8_t expected[32];
    sha256_hash(msg, len, expected);

    /* Split at 7-byte chunks. */
    sha256_t ctx;
    sha256_init(&ctx);
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = 7;
        if (offset + chunk > len) chunk = len - offset;
        sha256_update(&ctx, &msg[offset], chunk);
        offset += chunk;
    }
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- NULL parameter checks ---- */

TEST(test_null_params)
{
    sha256_t ctx;
    uint8_t hash[32];

    ASSERT_EQ(sha256_init(NULL), SHA256_ERR_PARAM);
    ASSERT_EQ(sha256_update(NULL, (const uint8_t *)"x", 1), SHA256_ERR_PARAM);
    ASSERT_EQ(sha256_final(NULL, hash), SHA256_ERR_PARAM);

    sha256_init(&ctx);
    ASSERT_EQ(sha256_final(&ctx, NULL), SHA256_ERR_PARAM);
}

/* ---- Zero-length update is harmless ---- */

TEST(test_zero_length_update)
{
    uint8_t expected[32];
    sha256_hash((const uint8_t *)"abc", 3, expected);

    sha256_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, NULL, 0);
    sha256_update(&ctx, (const uint8_t *)"abc", 3);
    sha256_update(&ctx, (const uint8_t *)"x", 0);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

/* ---- "The quick brown fox..." known answer ---- */

TEST(test_quick_brown_fox)
{
    static const uint8_t expected[32] = {
        0xD7, 0xA8, 0xFB, 0xB3, 0x07, 0xD7, 0x80, 0x94,
        0x69, 0xCA, 0x9A, 0xBC, 0xB0, 0x08, 0x2E, 0x4F,
        0x8D, 0x56, 0x51, 0xE4, 0x6D, 0x3C, 0xDB, 0x76,
        0x2D, 0x02, 0xD0, 0xBF, 0x37, 0xC9, 0xE5, 0x92,
    };
    const char *msg = "The quick brown fox jumps over the lazy dog";
    uint8_t hash[32];
    sha256_hash((const uint8_t *)msg, strlen(msg), hash);
    ASSERT_TRUE(hash_eq(hash, expected));
}

TEST_MAIN("sha256")
    RUN(test_abc);
    RUN(test_empty);
    RUN(test_448bit);
    RUN(test_million_a);
    RUN(test_streaming_equivalence);
    RUN(test_chunked_streaming);
    RUN(test_null_params);
    RUN(test_zero_length_update);
    RUN(test_quick_brown_fox);
TEST_MAIN_END()
