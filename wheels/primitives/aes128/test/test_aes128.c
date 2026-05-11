// SPDX-License-Identifier: MIT
/**
 * @file    test_aes128.c
 * @brief   C unit tests for aes128 wheel (NIST FIPS 197 test vectors).
 */

#include "test_harness.h"
#include "aes128.h"
#include <string.h>

/* ---- FIPS 197, Appendix B test vector ---- */

static const uint8_t fips197_key[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
};

static const uint8_t fips197_plaintext[16] = {
    0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D,
    0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07, 0x34,
};

static const uint8_t fips197_ciphertext[16] = {
    0x39, 0x25, 0x84, 0x1D, 0x02, 0xDC, 0x09, 0xFB,
    0xDC, 0x11, 0x85, 0x97, 0x19, 0x6A, 0x0B, 0x32,
};

TEST(test_fips197_encrypt)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, fips197_key), AES128_OK);
    ASSERT_EQ(aes128_encrypt_block(&ctx, fips197_plaintext, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, fips197_ciphertext, 16) == 0);
}

TEST(test_fips197_decrypt)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, fips197_key), AES128_OK);
    ASSERT_EQ(aes128_decrypt_block(&ctx, fips197_ciphertext, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, fips197_plaintext, 16) == 0);
}

/* ---- NIST SP 800-38A, Appendix F.1.1 (ECB-AES128 encrypt) ---- */

static const uint8_t nist_ecb_key[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
};

static const uint8_t nist_ecb_pt1[16] = {
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
    0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A,
};
static const uint8_t nist_ecb_ct1[16] = {
    0x3A, 0xD7, 0x7B, 0xB4, 0x0D, 0x7A, 0x36, 0x60,
    0xA8, 0x9E, 0xCA, 0xF3, 0x24, 0x66, 0xEF, 0x97,
};

static const uint8_t nist_ecb_pt2[16] = {
    0xAE, 0x2D, 0x8A, 0x57, 0x1E, 0x03, 0xAC, 0x9C,
    0x9E, 0xB7, 0x6F, 0xAC, 0x45, 0xAF, 0x8E, 0x51,
};
static const uint8_t nist_ecb_ct2[16] = {
    0xF5, 0xD3, 0xD5, 0x85, 0x03, 0xB9, 0x69, 0x9D,
    0xE7, 0x85, 0x89, 0x5A, 0x96, 0xFD, 0xBA, 0xAF,
};

TEST(test_nist_ecb_block1)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, nist_ecb_key), AES128_OK);
    ASSERT_EQ(aes128_encrypt_block(&ctx, nist_ecb_pt1, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, nist_ecb_ct1, 16) == 0);
}

TEST(test_nist_ecb_block2)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, nist_ecb_key), AES128_OK);
    ASSERT_EQ(aes128_encrypt_block(&ctx, nist_ecb_pt2, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, nist_ecb_ct2, 16) == 0);
}

TEST(test_nist_ecb_decrypt_block1)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, nist_ecb_key), AES128_OK);
    ASSERT_EQ(aes128_decrypt_block(&ctx, nist_ecb_ct1, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, nist_ecb_pt1, 16) == 0);
}

TEST(test_nist_ecb_decrypt_block2)
{
    aes128_ctx_t ctx;
    uint8_t out[16];
    ASSERT_EQ(aes128_init(&ctx, nist_ecb_key), AES128_OK);
    ASSERT_EQ(aes128_decrypt_block(&ctx, nist_ecb_ct2, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, nist_ecb_pt2, 16) == 0);
}

/* ---- Zero key + zero plaintext ---- */

TEST(test_zero_key_zero_plaintext)
{
    /* Known answer: AES-128 with all-zero key and all-zero plaintext. */
    static const uint8_t expected[16] = {
        0x66, 0xE9, 0x4B, 0xD4, 0xEF, 0x8A, 0x2C, 0x3B,
        0x88, 0x4C, 0xFA, 0x59, 0xCA, 0x34, 0x2B, 0x2E,
    };
    uint8_t key[16] = {0};
    uint8_t pt[16] = {0};
    uint8_t out[16];

    aes128_ctx_t ctx;
    ASSERT_EQ(aes128_init(&ctx, key), AES128_OK);
    ASSERT_EQ(aes128_encrypt_block(&ctx, pt, out), AES128_OK);
    ASSERT_TRUE(memcmp(out, expected, 16) == 0);
}

/* ---- NULL parameter checks ---- */

TEST(test_null_params)
{
    aes128_ctx_t ctx;
    uint8_t buf[16] = {0};
    uint8_t key[16] = {0};

    ASSERT_EQ(aes128_init(NULL, key), AES128_ERR_PARAM);
    ASSERT_EQ(aes128_init(&ctx, NULL), AES128_ERR_PARAM);

    ASSERT_EQ(aes128_init(&ctx, key), AES128_OK);
    ASSERT_EQ(aes128_encrypt_block(NULL, buf, buf), AES128_ERR_PARAM);
    ASSERT_EQ(aes128_encrypt_block(&ctx, NULL, buf), AES128_ERR_PARAM);
    ASSERT_EQ(aes128_encrypt_block(&ctx, buf, NULL), AES128_ERR_PARAM);

    ASSERT_EQ(aes128_decrypt_block(NULL, buf, buf), AES128_ERR_PARAM);
    ASSERT_EQ(aes128_decrypt_block(&ctx, NULL, buf), AES128_ERR_PARAM);
    ASSERT_EQ(aes128_decrypt_block(&ctx, buf, NULL), AES128_ERR_PARAM);
}

/* ---- In-place encrypt/decrypt (in == out) ---- */

TEST(test_in_place)
{
    aes128_ctx_t ctx;
    uint8_t buf[16];
    memcpy(buf, fips197_plaintext, 16);

    ASSERT_EQ(aes128_init(&ctx, fips197_key), AES128_OK);

    /* Encrypt in-place. */
    ASSERT_EQ(aes128_encrypt_block(&ctx, buf, buf), AES128_OK);
    ASSERT_TRUE(memcmp(buf, fips197_ciphertext, 16) == 0);

    /* Decrypt in-place. */
    ASSERT_EQ(aes128_decrypt_block(&ctx, buf, buf), AES128_OK);
    ASSERT_TRUE(memcmp(buf, fips197_plaintext, 16) == 0);
}

/* ---- Different keys produce different ciphertexts ---- */

TEST(test_different_keys)
{
    uint8_t key1[16] = {0};
    uint8_t key2[16] = {0};
    key2[0] = 1;  /* differ by one bit */

    uint8_t pt[16] = {0};
    uint8_t ct1[16], ct2[16];
    aes128_ctx_t ctx;

    aes128_init(&ctx, key1);
    aes128_encrypt_block(&ctx, pt, ct1);

    aes128_init(&ctx, key2);
    aes128_encrypt_block(&ctx, pt, ct2);

    ASSERT_TRUE(memcmp(ct1, ct2, 16) != 0);
}

/* ---- aes128_clear erases round-key schedule ---- */

TEST(test_clear)
{
    uint8_t key[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
                       0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
    aes128_ctx_t ctx;
    uint8_t zeroed[sizeof(aes128_ctx_t)];
    memset(zeroed, 0, sizeof(zeroed));

    ASSERT_EQ(aes128_init(&ctx, key), AES128_OK);

    /* Round keys must not be all-zero after init. */
    ASSERT_TRUE(memcmp(&ctx, zeroed, sizeof(ctx)) != 0);

    ASSERT_EQ(aes128_clear(&ctx), AES128_OK);

    /* All bytes of the context must be zero after clear. */
    ASSERT_TRUE(memcmp(&ctx, zeroed, sizeof(ctx)) == 0);

    /* NULL guard. */
    ASSERT_EQ(aes128_clear(NULL), AES128_ERR_PARAM);
}

TEST_MAIN("aes128")
    RUN(test_fips197_encrypt);
    RUN(test_fips197_decrypt);
    RUN(test_nist_ecb_block1);
    RUN(test_nist_ecb_block2);
    RUN(test_nist_ecb_decrypt_block1);
    RUN(test_nist_ecb_decrypt_block2);
    RUN(test_zero_key_zero_plaintext);
    RUN(test_null_params);
    RUN(test_in_place);
    RUN(test_different_keys);
    RUN(test_clear);
TEST_MAIN_END()
