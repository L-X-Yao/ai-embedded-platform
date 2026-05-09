/*
 * prop_moving_window.c — property-based tests for moving_window
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_moving_window.c ../moving_window.c -o prop_moving_window -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../moving_window.h"
#include <math.h>

#define WIN_CAP 32

/* Property: count never exceeds capacity */
PROPERTY(count_le_capacity, 10000) {
    float storage[WIN_CAP];
    moving_window_t w;
    moving_window_init(&w, storage, WIN_CAP);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 0, 100);
    for (unsigned i = 0; i < ops; i++) {
        float val = (float)ARBI_RANGE(seed, -1000, 1000) / 10.0f;
        moving_window_push(&w, val);
        PROP_ASSERT(moving_window_get_count(&w) <= WIN_CAP);
    }
}

/* Property: mean of uniform values equals the value */
PROPERTY(uniform_mean, 10000) {
    float storage[WIN_CAP];
    moving_window_t w;
    moving_window_init(&w, storage, WIN_CAP);

    float val = (float)ARBI_RANGE(seed, -500, 500);
    unsigned count = (unsigned)ARBI_RANGE(seed, 1, WIN_CAP);
    for (unsigned i = 0; i < count; i++) {
        moving_window_push(&w, val);
    }

    float mean = moving_window_get_mean(&w);
    PROP_ASSERT(fabsf(mean - val) < 0.01f);
}

/* Property: min <= mean <= max always holds */
PROPERTY(min_le_mean_le_max, 10000) {
    float storage[WIN_CAP];
    moving_window_t w;
    moving_window_init(&w, storage, WIN_CAP);

    unsigned count = (unsigned)ARBI_RANGE(seed, 1, WIN_CAP);
    for (unsigned i = 0; i < count; i++) {
        float val = (float)ARBI_RANGE(seed, -1000, 1000) / 10.0f;
        moving_window_push(&w, val);
    }

    float mn = moving_window_get_min(&w);
    float mx = moving_window_get_max(&w);
    float mean = moving_window_get_mean(&w);

    PROP_ASSERT(mn <= mean + 0.01f);
    PROP_ASSERT(mean <= mx + 0.01f);
}

/* Property: variance is non-negative */
PROPERTY(variance_nonneg, 10000) {
    float storage[WIN_CAP];
    moving_window_t w;
    moving_window_init(&w, storage, WIN_CAP);

    unsigned count = (unsigned)ARBI_RANGE(seed, 1, WIN_CAP);
    for (unsigned i = 0; i < count; i++) {
        float val = (float)ARBI_RANGE(seed, -1000, 1000) / 10.0f;
        moving_window_push(&w, val);
    }

    float var = moving_window_get_variance(&w);
    PROP_ASSERT(var >= -0.01f);  /* allow tiny float rounding */
}

/* Property: reset clears count to 0 */
PROPERTY(reset_clears, 5000) {
    float storage[WIN_CAP];
    moving_window_t w;
    moving_window_init(&w, storage, WIN_CAP);

    unsigned count = (unsigned)ARBI_RANGE(seed, 1, WIN_CAP);
    for (unsigned i = 0; i < count; i++) {
        moving_window_push(&w, (float)i);
    }

    moving_window_reset(&w);
    PROP_ASSERT_EQ(moving_window_get_count(&w), 0);
    PROP_ASSERT(!moving_window_is_full(&w));
}

TEST_MAIN("moving_window properties") {
    RUN(count_le_capacity);
    RUN(uniform_mean);
    RUN(min_le_mean_le_max);
    RUN(variance_nonneg);
    RUN(reset_clears);
}
TEST_MAIN_END()
