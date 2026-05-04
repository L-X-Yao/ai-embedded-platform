/*
 * prop_checksum.c — property-based tests for checksum
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_checksum.c ../checksum.c -o prop_checksum
 */

#include "test_harness.h"
#include "property_test.h"
#include "../checksum.h"
#include <string.h>

/* Property: same input always produces same checksum (deterministic) */
PROPERTY(xor_deterministic, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint8_t c1 = checksum_xor(data, len);
    uint8_t c2 = checksum_xor(data, len);
    PROP_ASSERT_EQ(c1, c2);
}

/* Property: XOR of a byte with itself is zero (involution) */
PROPERTY(xor_self_cancel, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, data, len);

    /* double the data: [data | data] — XOR should cancel */
    uint8_t doubled[128];
    memcpy(doubled, data, len);
    memcpy(doubled + len, data, len);

    uint8_t result = checksum_xor(doubled, len * 2);
    PROP_ASSERT_EQ(result, 0);
}

/* Property: all checksum functions are deterministic */
PROPERTY(all_deterministic, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    PROP_ASSERT_EQ(checksum_sum8(data, len), checksum_sum8(data, len));
    PROP_ASSERT_EQ(checksum_fletcher16(data, len), checksum_fletcher16(data, len));
    PROP_ASSERT_EQ(checksum_lrc(data, len), checksum_lrc(data, len));
    PROP_ASSERT_EQ(checksum_internet(data, len), checksum_internet(data, len));
}

/* Property: LRC is two's complement of sum8 */
PROPERTY(lrc_is_twos_complement_of_sum, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, data, len);

    uint8_t sum = checksum_sum8(data, len);
    uint8_t lrc = checksum_lrc(data, len);
    /* lrc = (~sum + 1) = -sum mod 256, so sum + lrc == 0 mod 256 */
    PROP_ASSERT_EQ((uint8_t)(sum + lrc), 0);
}

/* Property: flipping a bit changes at least one checksum */
PROPERTY(bit_flip_detected, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, data, len);

    uint8_t  xor_orig = checksum_xor(data, len);
    uint8_t  sum_orig = checksum_sum8(data, len);
    uint16_t f16_orig = checksum_fletcher16(data, len);

    size_t byte_idx = ARBI_SIZE(seed, len - 1);
    uint8_t bit_mask = (uint8_t)(1u << (ARBI_UINT8(seed) % 8));
    data[byte_idx] ^= bit_mask;

    uint8_t  xor_new = checksum_xor(data, len);
    uint8_t  sum_new = checksum_sum8(data, len);
    uint16_t f16_new = checksum_fletcher16(data, len);

    /* at least one should change — XOR always detects single bit flips */
    PROP_ASSERT(xor_orig != xor_new || sum_orig != sum_new || f16_orig != f16_new);
}

TEST_MAIN("checksum properties") {
    RUN(xor_deterministic);
    RUN(xor_self_cancel);
    RUN(all_deterministic);
    RUN(lrc_is_twos_complement_of_sum);
    RUN(bit_flip_detected);
}
TEST_MAIN_END()
