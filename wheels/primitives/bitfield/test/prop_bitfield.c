/*
 * prop_bitfield.c — property-based tests for bitfield
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_bitfield.c ../bitfield.c -o prop_bitfield
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bitfield.h"
#include <string.h>

/* Property: set then get returns same value */
PROPERTY(set_get_roundtrip, 10000) {
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    size_t data_len = (size_t)ARBI_RANGE(seed, 1, 16);

    /* Pick a valid bit range that fits */
    size_t total_bits = data_len * 8;
    size_t bit_count = (size_t)ARBI_RANGE(seed, 1, 32);
    if (bit_count > total_bits) bit_count = total_bits;
    size_t max_offset = total_bits - bit_count;
    size_t bit_offset = ARBI_SIZE(seed, max_offset);

    /* Generate value that fits in bit_count bits */
    uint32_t mask = (bit_count >= 32) ? UINT32_MAX : ((1u << bit_count) - 1u);
    uint32_t value = ARBI_UINT32(seed) & mask;

    int rc = bitfield_set(data, data_len, bit_offset, bit_count, value);
    PROP_ASSERT_EQ(rc, BITFIELD_OK);

    uint32_t readback = 0;
    rc = bitfield_get(data, data_len, bit_offset, bit_count, &readback);
    PROP_ASSERT_EQ(rc, BITFIELD_OK);
    PROP_ASSERT_EQ(readback, value);
}

/* Property: clear_bit then test_bit returns false */
PROPERTY(clear_then_test, 10000) {
    uint8_t data[8];
    size_t data_len = (size_t)ARBI_RANGE(seed, 1, 8);
    ARBI_FILL(seed, data, data_len);

    size_t total_bits = data_len * 8;
    size_t bit_idx = ARBI_SIZE(seed, total_bits - 1);

    bitfield_clear_bit(data, data_len, bit_idx);
    PROP_ASSERT(!bitfield_test_bit(data, data_len, bit_idx));
}

/* Property: set_bit then test_bit returns true */
PROPERTY(set_then_test, 10000) {
    uint8_t data[8];
    size_t data_len = (size_t)ARBI_RANGE(seed, 1, 8);
    memset(data, 0, data_len);

    size_t total_bits = data_len * 8;
    size_t bit_idx = ARBI_SIZE(seed, total_bits - 1);

    bitfield_set_bit(data, data_len, bit_idx);
    PROP_ASSERT(bitfield_test_bit(data, data_len, bit_idx));
}

/* Property: set only affects targeted bits — other bits unchanged */
PROPERTY(set_no_side_effects, 5000) {
    uint8_t data[8], backup[8];
    size_t data_len = (size_t)ARBI_RANGE(seed, 2, 8);
    ARBI_FILL(seed, data, data_len);
    memcpy(backup, data, data_len);

    size_t total_bits = data_len * 8;
    size_t bit_count = (size_t)ARBI_RANGE(seed, 1, 8);
    if (bit_count > total_bits) bit_count = total_bits;
    size_t max_offset = total_bits - bit_count;
    size_t bit_offset = ARBI_SIZE(seed, max_offset);

    uint32_t mask = (bit_count >= 32) ? UINT32_MAX : ((1u << bit_count) - 1u);
    uint32_t value = ARBI_UINT32(seed) & mask;
    bitfield_set(data, data_len, bit_offset, bit_count, value);

    /* Check bits outside the written range are unchanged */
    for (size_t b = 0; b < total_bits; b++) {
        if (b >= bit_offset && b < bit_offset + bit_count) continue;
        bool orig = (backup[b / 8] >> (7 - (b % 8))) & 1;
        bool cur  = bitfield_test_bit(data, data_len, b);
        PROP_ASSERT(orig == cur);
    }
}

/* Property: out-of-range returns error, never crashes */
PROPERTY(out_of_range_safe, 50000) {
    uint8_t data[4] = {0};
    uint32_t out = 0;
    size_t data_len = (size_t)ARBI_RANGE(seed, 1, 4);
    size_t offset = ARBI_SIZE(seed, 64);
    size_t count = ARBI_SIZE(seed, 64);

    /* These may return error codes, but must not crash */
    (void)bitfield_get(data, data_len, offset, count, &out);
    (void)bitfield_set(data, data_len, offset, count, ARBI_UINT32(seed));

    PROP_ASSERT(1); /* reached without crash */
}

TEST_MAIN("bitfield properties") {
    RUN(set_get_roundtrip);
    RUN(clear_then_test);
    RUN(set_then_test);
    RUN(set_no_side_effects);
    RUN(out_of_range_safe);
}
TEST_MAIN_END()
