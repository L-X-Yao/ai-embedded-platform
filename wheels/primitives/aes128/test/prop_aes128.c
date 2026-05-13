// SPDX-License-Identifier: MIT
/**
 * @file    prop_aes128.c
 * @brief   Property-based tests for aes128 wheel.
 *
 * Properties verified:
 *   1. Encrypt then decrypt = original (round-trip) for many inputs.
 *   2. Encrypting the same plaintext with the same key is deterministic.
 *   3. Different plaintexts produce different ciphertexts (avalanche).
 */

#include "test_harness.h"
#include "aes128.h"
#include <string.h>

/* Simple deterministic PRNG (xorshift32) for reproducible tests. */
static uint32_t prng_state = 0xDEADBEEF;

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

TEST(test_roundtrip_property)
{
    /* For 100 random key/plaintext pairs, encrypt then decrypt must
     * yield the original plaintext. */
    for (int trial = 0; trial < 100; trial++) {
        uint8_t key[16], pt[16], ct[16], recovered[16];
        fill_random(key, 16);
        fill_random(pt, 16);

        aes128_t ctx;
        aes128_init(&ctx, key);
        aes128_encrypt_block(&ctx, pt, ct);
        aes128_decrypt_block(&ctx, ct, recovered);

        ASSERT_TRUE(memcmp(pt, recovered, 16) == 0);
    }
}

TEST(test_determinism_property)
{
    /* Encrypting the same input twice must produce identical output. */
    for (int trial = 0; trial < 50; trial++) {
        uint8_t key[16], pt[16], ct1[16], ct2[16];
        fill_random(key, 16);
        fill_random(pt, 16);

        aes128_t ctx;
        aes128_init(&ctx, key);
        aes128_encrypt_block(&ctx, pt, ct1);
        aes128_encrypt_block(&ctx, pt, ct2);

        ASSERT_TRUE(memcmp(ct1, ct2, 16) == 0);
    }
}

TEST(test_avalanche_property)
{
    /* Flipping one bit in the plaintext should change many bits in the
     * ciphertext.  We check that at least 1 byte differs (a weak form
     * of the avalanche criterion, but sufficient as a sanity check). */
    for (int trial = 0; trial < 50; trial++) {
        uint8_t key[16], pt1[16], pt2[16], ct1[16], ct2[16];
        fill_random(key, 16);
        fill_random(pt1, 16);
        memcpy(pt2, pt1, 16);
        pt2[trial % 16] ^= 0x01;  /* flip one bit */

        aes128_t ctx;
        aes128_init(&ctx, key);
        aes128_encrypt_block(&ctx, pt1, ct1);
        aes128_encrypt_block(&ctx, pt2, ct2);

        ASSERT_TRUE(memcmp(ct1, ct2, 16) != 0);
    }
}

TEST_MAIN("aes128 properties")
    RUN(test_roundtrip_property);
    RUN(test_determinism_property);
    RUN(test_avalanche_property);
TEST_MAIN_END()
