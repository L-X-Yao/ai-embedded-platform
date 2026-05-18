// SPDX-License-Identifier: MIT
/*
 * prop_crc32.c — property-based tests for crc32
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_crc32.c ../crc32.c -o prop_crc32
 */

#include "test_harness.h"
#include "property_test.h"
#include "../crc32.h"
#include <string.h>

/* Property: same input always produces same CRC (deterministic) */
PROPERTY(deterministic, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint32_t crc1 = crc32_compute(data, len);
    uint32_t crc2 = crc32_compute(data, len);
    PROP_ASSERT_EQ(crc1, crc2);
}

/* Property: streaming update+finalize matches one-shot compute */
PROPERTY(streaming_matches_oneshot, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint32_t oneshot = crc32_compute(data, len);

    /* stream in random-sized chunks */
    uint32_t running = CRC32_INIT;
    size_t pos = 0;
    while (pos < len) {
        size_t chunk = ARBI_SIZE(seed, len - pos);
        if (chunk == 0) chunk = 1;
        running = crc32_update(running, data + pos, chunk);
        pos += chunk;
    }
    uint32_t streamed = crc32_finalize(running);

    PROP_ASSERT_EQ(streamed, oneshot);
}

/* Property: flipping a bit changes the CRC */
PROPERTY(bit_flip_detected, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, data, len);

    uint32_t crc_orig = crc32_compute(data, len);

    size_t byte_idx = ARBI_SIZE(seed, len - 1);
    uint8_t bit_mask = (uint8_t)(1u << (ARBI_UINT8(seed) % 8));
    data[byte_idx] ^= bit_mask;

    uint32_t crc_flipped = crc32_compute(data, len);
    PROP_ASSERT(crc_orig != crc_flipped);
}

/* Property: known test vector - CRC32 of "123456789" = 0xCBF43926 */
PROPERTY(known_vector, 1) {
    const uint8_t data[] = "123456789";
    uint32_t crc = crc32_compute(data, 9);
    PROP_ASSERT_EQ(crc, (long long)0xCBF43926u);
}

/* Property: single-byte update matches compute for 1-byte input */
PROPERTY(single_byte_consistency, 10000) {
    uint8_t byte = ARBI_UINT8(seed);

    uint32_t oneshot = crc32_compute(&byte, 1);
    uint32_t streamed = crc32_finalize(crc32_update(CRC32_INIT, &byte, 1));

    PROP_ASSERT_EQ(oneshot, streamed);
}

TEST_MAIN("crc32 properties") {
    RUN(deterministic);
    RUN(streaming_matches_oneshot);
    RUN(bit_flip_detected);
    RUN(known_vector);
    RUN(single_byte_consistency);
}
TEST_MAIN_END()
