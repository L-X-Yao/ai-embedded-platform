// SPDX-License-Identifier: MIT
/**
 * @file    prop_sha256.c
 * @brief   Property-based tests for sha256 wheel.
 *
 * Properties verified:
 *   1. Determinism: same input always produces same hash.
 *   2. Collision resistance (weak): different inputs produce different hashes.
 *   3. Length sensitivity: same content with different lengths produces
 *      different hashes.
 *   4. Streaming consistency: arbitrary chunking matches one-shot.
 */

#include "test_harness.h"
#include "sha256.h"
#include <string.h>

/* Simple deterministic PRNG (xorshift32) for reproducible tests. */
static uint32_t prng_state = 0xCAFEBABE;

static uint32_t xorshift32(void)
{
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static void fill_random(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(xorshift32() & 0xFF);
    }
}

TEST(test_determinism_property)
{
    /* Hashing the same data twice must produce identical digests. */
    for (int trial = 0; trial < 100; trial++) {
        uint8_t data[128];
        size_t len = (size_t)(xorshift32() % 128) + 1;
        fill_random(data, len);

        uint8_t h1[32], h2[32];
        sha256_hash(data, len, h1);
        sha256_hash(data, len, h2);

        ASSERT_TRUE(memcmp(h1, h2, 32) == 0);
    }
}

TEST(test_different_inputs_property)
{
    /* Different inputs must produce different hashes (with overwhelming
     * probability for a 256-bit hash). */
    for (int trial = 0; trial < 100; trial++) {
        uint8_t data1[64], data2[64];
        fill_random(data1, sizeof(data1));
        memcpy(data2, data1, sizeof(data1));
        data2[trial % 64] ^= 0x01;  /* flip one bit */

        uint8_t h1[32], h2[32];
        sha256_hash(data1, sizeof(data1), h1);
        sha256_hash(data2, sizeof(data2), h2);

        ASSERT_TRUE(memcmp(h1, h2, 32) != 0);
    }
}

TEST(test_length_sensitivity_property)
{
    /* "abc" and "abcd" must hash differently. */
    uint8_t h3[32], h4[32];
    sha256_hash((const uint8_t *)"abc", 3, h3);
    sha256_hash((const uint8_t *)"abcd", 4, h4);
    ASSERT_TRUE(memcmp(h3, h4, 32) != 0);

    /* Same pattern for random data. */
    for (int trial = 0; trial < 50; trial++) {
        uint8_t data[129];
        size_t len = (size_t)(xorshift32() % 128) + 1;
        fill_random(data, len + 1);

        uint8_t h_short[32], h_long[32];
        sha256_hash(data, len, h_short);
        sha256_hash(data, len + 1, h_long);

        ASSERT_TRUE(memcmp(h_short, h_long, 32) != 0);
    }
}

TEST(test_streaming_chunking_property)
{
    /* For random data and random chunk sizes, streaming must match
     * one-shot. */
    for (int trial = 0; trial < 50; trial++) {
        uint8_t data[256];
        size_t len = (size_t)(xorshift32() % 256) + 1;
        fill_random(data, len);

        uint8_t oneshot[32];
        sha256_hash(data, len, oneshot);

        sha256_t ctx;
        sha256_init(&ctx);
        size_t offset = 0;
        while (offset < len) {
            size_t chunk = (size_t)(xorshift32() % 32) + 1;
            if (offset + chunk > len) chunk = len - offset;
            sha256_update(&ctx, &data[offset], chunk);
            offset += chunk;
        }
        uint8_t streaming[32];
        sha256_final(&ctx, streaming);

        ASSERT_TRUE(memcmp(oneshot, streaming, 32) == 0);
    }
}

TEST_MAIN("sha256 properties")
    RUN(test_determinism_property);
    RUN(test_different_inputs_property);
    RUN(test_length_sensitivity_property);
    RUN(test_streaming_chunking_property);
TEST_MAIN_END()
