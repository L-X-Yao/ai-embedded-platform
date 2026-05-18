// SPDX-License-Identifier: MIT
/*
 * prop_hash_fnv.c — property-based tests for hash_fnv
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_hash_fnv.c ../hash_fnv.c -o prop_hash_fnv
 */

#include "test_harness.h"
#include "property_test.h"
#include "../hash_fnv.h"
#include <string.h>

/* Property: deterministic — same input always produces same hash */
PROPERTY(deterministic_32, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63);
    ARBI_FILL(seed, data, len);

    uint32_t h1 = fnv1a_32(data, len);
    uint32_t h2 = fnv1a_32(data, len);
    PROP_ASSERT_EQ(h1, h2);
}

/* Property: deterministic — same input always produces same 64-bit hash */
PROPERTY(deterministic_64, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63);
    ARBI_FILL(seed, data, len);

    uint64_t h1 = fnv1a_64(data, len);
    uint64_t h2 = fnv1a_64(data, len);
    PROP_ASSERT(h1 == h2);
}

/* Property: incremental update equals single-shot */
PROPERTY(incremental_eq_single, 10000) {
    uint8_t data[64];
    size_t total = ARBI_SIZE(seed, 63);
    ARBI_FILL(seed, data, total);

    /* Split at random point */
    size_t split = ARBI_SIZE(seed, total);

    uint32_t single = fnv1a_32(data, total);

    uint32_t inc = FNV1A_32_OFFSET_BASIS;
    inc = fnv1a_32_update(inc, data, split);
    inc = fnv1a_32_update(inc, data + split, total - split);

    PROP_ASSERT_EQ(single, inc);
}

/* Property: distribution — no collisions in a small set of distinct inputs */
PROPERTY(no_collisions_small_set, 5000) {
    uint32_t hashes[32];

    for (int i = 0; i < 32; i++) {
        uint8_t data[8];
        /* Each input is unique: embed index + random bytes */
        data[0] = (uint8_t)i;
        data[1] = (uint8_t)(i >> 8);
        size_t extra = ARBI_SIZE(seed, 5);
        ARBI_FILL(seed, data + 2, extra);
        /* Ensure at least 3 bytes for uniqueness */
        data[2] = (uint8_t)(ARBI_UINT8(seed) ^ (uint8_t)i);
        hashes[i] = fnv1a_32(data, 3 + extra);
    }

    /* Check pairwise — FNV-1a should not collide on 32 distinct short inputs */
    int collisions = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = i + 1; j < 32; j++) {
            if (hashes[i] == hashes[j]) collisions++;
        }
    }
    /* Allow at most 1 collision (birthday paradox unlikely with 32 items) */
    PROP_ASSERT(collisions <= 1);
}

/* Property: string hash equals byte hash of string content */
PROPERTY(string_eq_bytes, 10000) {
    char str[32];
    size_t len = ARBI_SIZE(seed, 30);
    ARBI_FILL(seed, (uint8_t *)str, len);
    /* Ensure no NUL within the string */
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0') str[i] = 'A';
    }
    str[len] = '\0';

    uint32_t h_str = fnv1a_32_string(str);
    uint32_t h_bytes = fnv1a_32((const uint8_t *)str, len);
    PROP_ASSERT_EQ(h_str, h_bytes);
}

TEST_MAIN("hash_fnv properties") {
    RUN(deterministic_32);
    RUN(deterministic_64);
    RUN(incremental_eq_single);
    RUN(no_collisions_small_set);
    RUN(string_eq_bytes);
}
TEST_MAIN_END()
