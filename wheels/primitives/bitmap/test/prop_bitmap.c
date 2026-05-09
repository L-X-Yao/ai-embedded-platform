/*
 * prop_bitmap.c — property-based tests for bitmap
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_bitmap.c ../bitmap.c -o prop_bitmap
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bitmap.h"
#include <string.h>

#define MAX_BITS 128

/* Property: set-get roundtrip — any bit set can be read back */
PROPERTY(set_get_roundtrip, 10000) {
    uint8_t storage[(MAX_BITS + 7) / 8];
    bitmap_t bm;
    uint16_t bits = (uint16_t)ARBI_RANGE(seed, 1, MAX_BITS);
    bitmap_init(&bm, storage, bits);

    uint16_t bit = (uint16_t)(ARBI_UINT16(seed) % bits);
    bitmap_set(&bm, bit);
    PROP_ASSERT(bitmap_test(&bm, bit));
}

/* Property: clear-get returns false */
PROPERTY(clear_get_false, 10000) {
    uint8_t storage[(MAX_BITS + 7) / 8];
    bitmap_t bm;
    uint16_t bits = (uint16_t)ARBI_RANGE(seed, 1, MAX_BITS);
    bitmap_init(&bm, storage, bits);

    uint16_t bit = (uint16_t)(ARBI_UINT16(seed) % bits);
    bitmap_set(&bm, bit);
    bitmap_clear(&bm, bit);
    PROP_ASSERT(!bitmap_test(&bm, bit));
}

/* Property: count_set + count_clear == total bits */
PROPERTY(count_invariant, 10000) {
    uint8_t storage[(MAX_BITS + 7) / 8];
    bitmap_t bm;
    uint16_t bits = (uint16_t)ARBI_RANGE(seed, 1, MAX_BITS);
    bitmap_init(&bm, storage, bits);

    /* Set some random bits */
    unsigned ops = (unsigned)ARBI_RANGE(seed, 0, 50);
    for (unsigned i = 0; i < ops; i++) {
        uint16_t bit = (uint16_t)(ARBI_UINT16(seed) % bits);
        if (ARBI_UINT8(seed) % 2)
            bitmap_set(&bm, bit);
        else
            bitmap_clear(&bm, bit);
    }

    PROP_ASSERT_EQ(bitmap_count_set(&bm) + bitmap_count_clear(&bm), bits);
}

/* Property: alloc returns unique indices until full */
PROPERTY(alloc_unique, 5000) {
    uint8_t storage[(MAX_BITS + 7) / 8];
    bitmap_t bm;
    uint16_t bits = (uint16_t)ARBI_RANGE(seed, 1, 64);
    bitmap_init(&bm, storage, bits);

    uint16_t allocated[64];
    int count = 0;

    for (uint16_t i = 0; i < bits; i++) {
        int idx = bitmap_alloc(&bm);
        PROP_ASSERT(idx >= 0);
        /* Check uniqueness */
        for (int j = 0; j < count; j++) {
            PROP_ASSERT(allocated[j] != (uint16_t)idx);
        }
        allocated[count++] = (uint16_t)idx;
    }

    /* After allocating all, next alloc should fail */
    PROP_ASSERT_EQ(bitmap_alloc(&bm), -1);
}

/* Property: out-of-range operations return error / false safely */
PROPERTY(out_of_range_safe, 10000) {
    uint8_t storage[(MAX_BITS + 7) / 8];
    bitmap_t bm;
    uint16_t bits = (uint16_t)ARBI_RANGE(seed, 1, MAX_BITS);
    bitmap_init(&bm, storage, bits);

    /* Out-of-range bit */
    uint16_t oob = bits + (uint16_t)ARBI_RANGE(seed, 0, 100);
    PROP_ASSERT(!bitmap_test(&bm, oob));
    PROP_ASSERT(bitmap_set(&bm, oob) != BITMAP_OK);
    PROP_ASSERT(bitmap_clear(&bm, oob) != BITMAP_OK);
}

TEST_MAIN("bitmap properties") {
    RUN(set_get_roundtrip);
    RUN(clear_get_false);
    RUN(count_invariant);
    RUN(alloc_unique);
    RUN(out_of_range_safe);
}
TEST_MAIN_END()
