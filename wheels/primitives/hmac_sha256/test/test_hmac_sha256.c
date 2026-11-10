// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <stddef.h>
#include <string.h>
#include "hmac_sha256.h"
#include "sha256.h"

static void hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    for (size_t i = 0; i < out_len; i++) {
        unsigned val;
        sscanf(hex + 2 * i, "%2x", &val);
        out[i] = (uint8_t)val;
    }
}

/* ---------- RFC 4231 test vectors ---------- */

/* Test Case 1: key = 20 bytes of 0x0b */
TEST(test_rfc4231_case1)
{
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    const char *data = "Hi There";
    uint8_t mac[32];

    hmac_sha256_compute(key, 20, (const uint8_t *)data, 8, mac);

    uint8_t expected[32];
    hex_to_bytes("b0344c61d8db38535ca8afceaf0bf12b"
                 "881dc200c9833da726e9376c2e32cff7",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 2: key = "Jefe" */
TEST(test_rfc4231_case2)
{
    const char *key = "Jefe";
    const char *data = "what do ya want for nothing?";
    uint8_t mac[32];

    hmac_sha256_compute((const uint8_t *)key, 4,
                        (const uint8_t *)data, 28, mac);

    uint8_t expected[32];
    hex_to_bytes("5bdcc146bf60754e6a042426089575c7"
                 "5a003f089d2739839dec58b964ec3843",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 3: key = 20 bytes of 0xaa, data = 50 bytes of 0xdd */
TEST(test_rfc4231_case3)
{
    uint8_t key[20];
    memset(key, 0xaa, sizeof(key));
    uint8_t data[50];
    memset(data, 0xdd, sizeof(data));
    uint8_t mac[32];

    hmac_sha256_compute(key, 20, data, 50, mac);

    uint8_t expected[32];
    hex_to_bytes("773ea91e36800e46854db8ebd09181a7"
                 "2959098b3ef8c122d9635514ced565fe",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 4: key = 25 bytes (01..19), data = 50 bytes of 0xcd */
TEST(test_rfc4231_case4)
{
    uint8_t key[25];
    for (size_t i = 0; i < 25; i++) key[i] = (uint8_t)(i + 1);
    uint8_t data[50];
    memset(data, 0xcd, sizeof(data));
    uint8_t mac[32];

    hmac_sha256_compute(key, 25, data, 50, mac);

    uint8_t expected[32];
    hex_to_bytes("82558a389a443c0ea4cc819899f2083a"
                 "85f0faa3e578f8077a2e3ff46729665b",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 5: key = 20 bytes of 0x0c, data = "Test With Truncation" */
TEST(test_rfc4231_case5)
{
    uint8_t key[20];
    memset(key, 0x0c, sizeof(key));
    const char *data = "Test With Truncation";
    uint8_t mac[32];

    hmac_sha256_compute(key, 20, (const uint8_t *)data, 20, mac);

    uint8_t expected[32];
    hex_to_bytes("a3b6167473100ee06e0c796c2955552b"
                 "fa6f7c0a6a8aef8b93f860aab0cd20c5",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 6: key = 131 bytes of 0xaa (> block size, hashed first) */
TEST(test_rfc4231_case6_long_key)
{
    uint8_t key[131];
    memset(key, 0xaa, sizeof(key));
    const char *data = "Test Using Larger Than Block-Size Key - Hash Key First";
    uint8_t mac[32];

    hmac_sha256_compute(key, 131, (const uint8_t *)data, 54, mac);

    uint8_t expected[32];
    hex_to_bytes("60e431591ee0b67f0d8a26aacbf5b77f"
                 "8e0bc6213728c5140546040f0ee37f54",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* Test Case 7: key = 131 bytes of 0xaa, longer data */
TEST(test_rfc4231_case7_long_key_long_data)
{
    uint8_t key[131];
    memset(key, 0xaa, sizeof(key));
    const char *data = "This is a test using a larger than block-size key "
                       "and a larger than block-size data. The key needs to "
                       "be hashed before being used by the HMAC algorithm.";
    uint8_t mac[32];

    hmac_sha256_compute(key, 131, (const uint8_t *)data, 152, mac);

    uint8_t expected[32];
    hex_to_bytes("9b09ffa71b942fcb27635fbcd5b0e944"
                 "bfdc63644f0713938a7f51535c3a35e2",
                 expected, 32);
    ASSERT_TRUE(hmac_sha256_verify(mac, expected));
}

/* ---------- streaming interface ---------- */

TEST(test_streaming_matches_oneshot)
{
    const char *key = "secret";
    const char *msg = "hello world this is a longer message for streaming";
    uint8_t mac_oneshot[32], mac_stream[32];

    hmac_sha256_compute((const uint8_t *)key, 6,
                        (const uint8_t *)msg, strlen(msg), mac_oneshot);

    hmac_sha256_t ctx;
    ASSERT_TRUE(hmac_sha256_init(&ctx, (const uint8_t *)key, 6) == HMAC_SHA256_OK);
    ASSERT_TRUE(hmac_sha256_update(&ctx, (const uint8_t *)msg, 10) == HMAC_SHA256_OK);
    ASSERT_TRUE(hmac_sha256_update(&ctx, (const uint8_t *)msg + 10, strlen(msg) - 10) == HMAC_SHA256_OK);
    ASSERT_TRUE(hmac_sha256_final(&ctx, mac_stream) == HMAC_SHA256_OK);

    ASSERT_TRUE(hmac_sha256_verify(mac_oneshot, mac_stream));
}

/* ---------- empty message ---------- */

TEST(test_empty_message)
{
    uint8_t key[16];
    memset(key, 0x42, sizeof(key));
    uint8_t mac[32];

    hmac_sha256_compute(key, 16, NULL, 0, mac);

    hmac_sha256_t ctx;
    hmac_sha256_init(&ctx, key, 16);
    uint8_t mac2[32];
    hmac_sha256_final(&ctx, mac2);

    ASSERT_TRUE(hmac_sha256_verify(mac, mac2));
}

/* ---------- empty key ---------- */

TEST(test_empty_key)
{
    const char *data = "test";
    uint8_t mac[32];
    hmac_sha256_compute(NULL, 0, (const uint8_t *)data, 4, mac);

    hmac_sha256_t ctx;
    hmac_sha256_init(&ctx, NULL, 0);
    hmac_sha256_update(&ctx, (const uint8_t *)data, 4);
    uint8_t mac2[32];
    hmac_sha256_final(&ctx, mac2);

    ASSERT_TRUE(hmac_sha256_verify(mac, mac2));
}

/* ---------- verify reject ---------- */

TEST(test_verify_rejects_mismatch)
{
    uint8_t a[32] = {0};
    uint8_t b[32] = {0};
    ASSERT_TRUE(hmac_sha256_verify(a, b));

    b[15] = 0xff;
    ASSERT_TRUE(!hmac_sha256_verify(a, b));
}

/* ---------- NULL guards ---------- */

TEST(test_null_ctx_init)
{
    uint8_t key[4] = {0};
    ASSERT_TRUE(hmac_sha256_init(NULL, key, 4) == HMAC_SHA256_ERR_PARAM);
}

TEST(test_null_ctx_update)
{
    uint8_t data[4] = {0};
    ASSERT_TRUE(hmac_sha256_update(NULL, data, 4) == HMAC_SHA256_ERR_PARAM);
}

TEST(test_null_ctx_final)
{
    uint8_t mac[32];
    ASSERT_TRUE(hmac_sha256_final(NULL, mac) == HMAC_SHA256_ERR_PARAM);
}

TEST(test_null_mac_final)
{
    hmac_sha256_t ctx;
    hmac_sha256_init(&ctx, NULL, 0);
    ASSERT_TRUE(hmac_sha256_final(&ctx, NULL) == HMAC_SHA256_ERR_PARAM);
}

TEST(test_null_data_nonzero_len)
{
    hmac_sha256_t ctx;
    hmac_sha256_init(&ctx, NULL, 0);
    ASSERT_TRUE(hmac_sha256_update(&ctx, NULL, 5) == HMAC_SHA256_ERR_PARAM);
}

TEST(test_null_key_nonzero_len)
{
    hmac_sha256_t ctx;
    ASSERT_TRUE(hmac_sha256_init(&ctx, NULL, 5) == HMAC_SHA256_ERR_PARAM);
}

/* compute() returns nothing, so a bad argument has to show in the output:
 * no key or message means there is no MAC to give. */
TEST(test_compute_null_key_nonzero_len_zeroes_mac)
{
    static const uint8_t zero[32] = {0};
    uint8_t mac[32];

    memset(mac, 0xA5, sizeof(mac));
    hmac_sha256_compute(NULL, 5, (const uint8_t *)"msg", 3, mac);
    ASSERT_MEMEQ_N(mac, zero, sizeof(mac));
}

TEST(test_compute_null_data_nonzero_len_zeroes_mac)
{
    static const uint8_t zero[32] = {0};
    uint8_t mac[32];

    memset(mac, 0xA5, sizeof(mac));
    hmac_sha256_compute((const uint8_t *)"key", 3, NULL, 100, mac);
    ASSERT_MEMEQ_N(mac, zero, sizeof(mac));
}

TEST(test_verify_null_args)
{
    uint8_t a[32] = {0};
    ASSERT_TRUE(!hmac_sha256_verify(NULL, a));
    ASSERT_TRUE(!hmac_sha256_verify(a, NULL));
    ASSERT_TRUE(!hmac_sha256_verify(NULL, NULL));
}

/* ---------- Where a context can live ---------- */

/* A byte before a member pushes it to a multiple of its type's alignment,
 * so the member's offset here is that alignment. */
typedef struct {
    uint8_t       tag;
    hmac_sha256_t ctx;
} hmac_after_byte_t;

typedef struct {
    uint8_t  tag;
    sha256_t ctx;
} sha256_after_byte_t;

TEST(test_context_aligns_like_the_sha256_it_holds)
{
    ASSERT_EQ(offsetof(sha256_after_byte_t, ctx),
              offsetof(hmac_after_byte_t, ctx));
}

/* RFC 4231 case 2 from a context inside a caller's struct. A context with
 * alignment 1 lands at offset 1 here: UBSan reports it, a Cortex-M faults. */
TEST(test_context_after_a_byte_gives_the_rfc4231_mac)
{
    static hmac_after_byte_t rec;
    const char *data = "what do ya want for nothing?";
    uint8_t mac[32];
    uint8_t expected[32];

    hex_to_bytes("5bdcc146bf60754e6a042426089575c7"
                 "5a003f089d2739839dec58b964ec3843",
                 expected, 32);

    ASSERT_EQ(HMAC_SHA256_OK,
              hmac_sha256_init(&rec.ctx, (const uint8_t *)"Jefe", 4));
    ASSERT_EQ(HMAC_SHA256_OK,
              hmac_sha256_update(&rec.ctx, (const uint8_t *)data, 28));
    ASSERT_EQ(HMAC_SHA256_OK, hmac_sha256_final(&rec.ctx, mac));
    ASSERT_MEMEQ_N(expected, mac, 32);
}

TEST_MAIN("hmac_sha256")
    RUN(test_rfc4231_case1);
    RUN(test_rfc4231_case2);
    RUN(test_rfc4231_case3);
    RUN(test_rfc4231_case4);
    RUN(test_rfc4231_case5);
    RUN(test_rfc4231_case6_long_key);
    RUN(test_rfc4231_case7_long_key_long_data);
    RUN(test_streaming_matches_oneshot);
    RUN(test_empty_message);
    RUN(test_empty_key);
    RUN(test_verify_rejects_mismatch);
    RUN(test_null_ctx_init);
    RUN(test_null_ctx_update);
    RUN(test_null_ctx_final);
    RUN(test_null_mac_final);
    RUN(test_null_data_nonzero_len);
    RUN(test_null_key_nonzero_len);
    RUN(test_compute_null_key_nonzero_len_zeroes_mac);
    RUN(test_compute_null_data_nonzero_len_zeroes_mac);
    RUN(test_verify_null_args);
    RUN(test_context_aligns_like_the_sha256_it_holds);
    RUN(test_context_after_a_byte_gives_the_rfc4231_mac);
TEST_MAIN_END()
