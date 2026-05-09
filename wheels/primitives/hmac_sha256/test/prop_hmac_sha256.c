/*
 * prop_hmac_sha256.c -- property-based tests for hmac_sha256
 *
 * Build: gcc -std=c99 -I.. -I../../sha256 -I../../../../tooling/harness
 *        prop_hmac_sha256.c ../hmac_sha256.c ../../sha256/sha256.c
 *        -o prop_hmac_sha256
 */

#include "test_harness.h"
#include "property_test.h"
#include "../hmac_sha256.h"
#include <string.h>

/* --- Property 1: one-shot and streaming produce identical MAC --- */

PROPERTY(oneshot_equals_streaming, 10000) {
    uint8_t key[128];
    size_t key_len = ARBI_SIZE(seed, 127);
    ARBI_FILL(seed, key, key_len);

    uint8_t data[256];
    size_t data_len = ARBI_SIZE(seed, 255);
    ARBI_FILL(seed, data, data_len);

    /* one-shot */
    uint8_t mac_one[32];
    hmac_sha256_compute(key, key_len, data, data_len, mac_one);

    /* streaming: split data at a random point */
    size_t split = (data_len > 0) ? ARBI_SIZE(seed, data_len - 1) : 0;
    hmac_sha256_t ctx;
    hmac_sha256_init(&ctx, key, key_len);
    hmac_sha256_update(&ctx, data, split);
    hmac_sha256_update(&ctx, data + split, data_len - split);
    uint8_t mac_str[32];
    hmac_sha256_final(&ctx, mac_str);

    PROP_ASSERT_MEM_EQ(mac_one, mac_str, 32);
}

/* --- Property 2: deterministic -- same input always same output --- */

PROPERTY(deterministic, 10000) {
    uint8_t key[64];
    size_t key_len = ARBI_SIZE(seed, 63);
    ARBI_FILL(seed, key, key_len);

    uint8_t data[128];
    size_t data_len = ARBI_SIZE(seed, 127);
    ARBI_FILL(seed, data, data_len);

    uint8_t mac1[32], mac2[32];
    hmac_sha256_compute(key, key_len, data, data_len, mac1);
    hmac_sha256_compute(key, key_len, data, data_len, mac2);

    PROP_ASSERT_MEM_EQ(mac1, mac2, 32);
}

/* --- Property 3: verify accepts correct MAC, rejects flipped bit --- */

PROPERTY(verify_correct_rejects_flipped, 10000) {
    uint8_t key[32];
    size_t key_len = ARBI_SIZE(seed, 31) + 1;
    ARBI_FILL(seed, key, key_len);

    uint8_t data[64];
    size_t data_len = ARBI_SIZE(seed, 63);
    ARBI_FILL(seed, data, data_len);

    uint8_t mac[32];
    hmac_sha256_compute(key, key_len, data, data_len, mac);

    PROP_ASSERT(hmac_sha256_verify(mac, mac));

    /* flip one random bit */
    uint8_t tampered[32];
    memcpy(tampered, mac, 32);
    uint8_t byte_idx = ARBI_UINT8(seed) % 32;
    uint8_t bit_idx  = ARBI_UINT8(seed) % 8;
    tampered[byte_idx] ^= (uint8_t)(1u << bit_idx);

    PROP_ASSERT(!hmac_sha256_verify(mac, tampered));
}

/* --- Property 4: different keys produce different MACs --- */

PROPERTY(different_keys_different_macs, 10000) {
    uint8_t key1[32], key2[32];
    ARBI_FILL(seed, key1, 32);
    ARBI_FILL(seed, key2, 32);

    /* ensure keys differ */
    key2[0] ^= 0x01;

    uint8_t data[64];
    size_t data_len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, data, data_len);

    uint8_t mac1[32], mac2[32];
    hmac_sha256_compute(key1, 32, data, data_len, mac1);
    hmac_sha256_compute(key2, 32, data, data_len, mac2);

    PROP_ASSERT(!hmac_sha256_verify(mac1, mac2));
}

/* --- Property 5: context reuse after final works correctly --- */

PROPERTY(context_reuse_after_final, 5000) {
    uint8_t key[16];
    ARBI_FILL(seed, key, 16);

    uint8_t data1[32], data2[32];
    size_t len1 = ARBI_SIZE(seed, 31) + 1;
    size_t len2 = ARBI_SIZE(seed, 31) + 1;
    ARBI_FILL(seed, data1, len1);
    ARBI_FILL(seed, data2, len2);

    hmac_sha256_t ctx;
    uint8_t mac1[32], mac2[32];

    /* first use */
    hmac_sha256_init(&ctx, key, 16);
    hmac_sha256_update(&ctx, data1, len1);
    hmac_sha256_final(&ctx, mac1);

    /* reuse same context with re-init */
    hmac_sha256_init(&ctx, key, 16);
    hmac_sha256_update(&ctx, data2, len2);
    hmac_sha256_final(&ctx, mac2);

    /* verify second result matches one-shot */
    uint8_t mac_ref[32];
    hmac_sha256_compute(key, 16, data2, len2, mac_ref);
    PROP_ASSERT_MEM_EQ(mac2, mac_ref, 32);
}

TEST_MAIN("hmac_sha256 properties") {
    RUN(oneshot_equals_streaming);
    RUN(deterministic);
    RUN(verify_correct_rejects_flipped);
    RUN(different_keys_different_macs);
    RUN(context_reuse_after_final);
}
TEST_MAIN_END()
