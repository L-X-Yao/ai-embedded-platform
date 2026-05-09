/*
 * prop_median_filter.c — property-based tests for median_filter
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_median_filter.c ../median_filter.c -o prop_median_filter
 */

#include "test_harness.h"
#include "property_test.h"
#include "../median_filter.h"

/* Property: output always within [min_input, max_input] of fed samples */
PROPERTY(output_bounded, 10000) {
    median_filter_t f;
    uint8_t size = (uint8_t)(3 + 2 * ARBI_SIZE(seed, 13));  /* odd: 3,5,...29 */
    if (size > MEDIAN_FILTER_MAX_SIZE) size = MEDIAN_FILTER_MAX_SIZE;
    median_filter_init(&f, size);

    int32_t lo = INT32_MAX, hi = INT32_MIN;
    unsigned n_samples = (unsigned)ARBI_RANGE(seed, 1, 50);

    for (unsigned i = 0; i < n_samples; i++) {
        int32_t sample = (int32_t)ARBI_RANGE(seed, -10000, 10000);
        if (sample < lo) lo = sample;
        if (sample > hi) hi = sample;
        int32_t median = median_filter_update(&f, sample);

        PROP_ASSERT(median >= lo);
        PROP_ASSERT(median <= hi);
    }
}

/* Property: single repeated value -> output equals that value */
PROPERTY(single_value_passthrough, 10000) {
    median_filter_t f;
    uint8_t size = (uint8_t)(3 + 2 * ARBI_SIZE(seed, 13));
    if (size > MEDIAN_FILTER_MAX_SIZE) size = MEDIAN_FILTER_MAX_SIZE;
    median_filter_init(&f, size);

    int32_t val = (int32_t)ARBI_RANGE(seed, -10000, 10000);

    /* Feed the same value enough to fill the window */
    int32_t result = 0;
    for (uint8_t i = 0; i < size; i++) {
        result = median_filter_update(&f, val);
    }

    PROP_ASSERT_EQ(result, val);
}

/* Property: get without new sample returns same as last update */
PROPERTY(get_matches_last_update, 5000) {
    median_filter_t f;
    median_filter_init(&f, 5);

    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 20);
    int32_t last_result = 0;
    for (unsigned i = 0; i < n; i++) {
        int32_t sample = (int32_t)ARBI_RANGE(seed, -5000, 5000);
        last_result = median_filter_update(&f, sample);
    }

    PROP_ASSERT_EQ(median_filter_get(&f), last_result);
}

/* Property: reset then feed -> same as fresh init then feed */
PROPERTY(reset_equivalent_to_init, 5000) {
    median_filter_t f1, f2;
    uint8_t size = 5;
    median_filter_init(&f1, size);
    median_filter_init(&f2, size);

    /* Feed some garbage into f1 */
    for (int i = 0; i < 10; i++) {
        median_filter_update(&f1, (int32_t)ARBI_RANGE(seed, -1000, 1000));
    }

    /* Reset f1 */
    median_filter_reset(&f1);

    /* Now feed identical sequence to both */
    uint32_t seq_seed = seed;
    uint32_t s1 = seq_seed, s2 = seq_seed;
    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 15);
    /* Regenerate n from a separate seed to keep s1/s2 in sync */
    for (unsigned i = 0; i < n; i++) {
        int32_t sample1 = (int32_t)ARBI_RANGE(s1, -1000, 1000);
        int32_t sample2 = (int32_t)ARBI_RANGE(s2, -1000, 1000);
        int32_t r1 = median_filter_update(&f1, sample1);
        int32_t r2 = median_filter_update(&f2, sample2);
        PROP_ASSERT_EQ(r1, r2);
    }
}

TEST_MAIN("median_filter properties") {
    RUN(output_bounded);
    RUN(single_value_passthrough);
    RUN(get_matches_last_update);
    RUN(reset_equivalent_to_init);
}
TEST_MAIN_END()
