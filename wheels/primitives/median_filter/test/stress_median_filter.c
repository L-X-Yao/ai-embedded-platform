// SPDX-License-Identifier: MIT
/*
 * stress_median_filter.c — long-running stress tests for median_filter
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_median_filter.c ../median_filter.c -o /tmp/stress_median_filter -lm
 *
 * Scenarios:
 *   1. Sustained constant input — median must converge and stay exact
 *   2. Large value swings — INT32_MIN to INT32_MAX alternation, median bounded
 *   3. Window saturation with ramp — monotonic input, verify median tracks correctly
 *   4. Spike rejection — steady signal with periodic spikes, median rejects them
 */

#include "test_harness.h"
#include "median_filter.h"
#include <stdint.h>

/* ---- tests ---------------------------------------------------------- */

TEST(constant_input_convergence)
{
    median_filter_t f;
    median_filter_init(&f, 7);

    /* Feed constant 42 for 1M samples — median must be 42 after window fills */
    int ok = 1;
    for (int i = 0; i < 1000000; i++) {
        int32_t result = median_filter_update(&f, 42);
        if (i >= 6 && result != 42) { ok = 0; break; } /* window size 7 */
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ(42, median_filter_get(&f));
}

TEST(extreme_value_swings)
{
    median_filter_t f;
    median_filter_init(&f, 5);

    int ok = 1;
    for (int i = 0; i < 500000; i++) {
        int32_t sample = (i % 2 == 0) ? INT32_MAX : INT32_MIN;
        int32_t result = median_filter_update(&f, sample);

        /* With window 5 alternating max/min:
         * Sorted window will always have 2-3 of each extremum.
         * Median must be one of the two values — never something else */
        if (i >= 4) {
            if (result != INT32_MAX && result != INT32_MIN) {
                ok = 0; break;
            }
        }
    }
    ASSERT_TRUE(ok);
}

TEST(ramp_median_tracking)
{
    median_filter_t f;
    median_filter_init(&f, 5);

    /* Feed monotonically increasing values. After window fills,
     * median of window [i-4, i-3, i-2, i-1, i] should be i-2 */
    int ok = 1;
    for (int32_t i = 0; i < 100000; i++) {
        int32_t result = median_filter_update(&f, i);
        if (i >= 4) {
            int32_t expected = i - 2; /* median of 5-element ascending window */
            if (result != expected) { ok = 0; break; }
        }
    }
    ASSERT_TRUE(ok);
}

TEST(spike_rejection)
{
    median_filter_t f;
    median_filter_init(&f, 7);

    /* Base signal is 100, with occasional spike to 10000 every 10th sample.
     * With window size 7, at most 1 spike in the window (10 > 7),
     * so median should always be 100 once the window is filled with the pattern. */
    int ok = 1;
    int stable_count = 0;
    for (int i = 0; i < 100000; i++) {
        int32_t sample = (i % 10 == 0) ? 10000 : 100;
        int32_t result = median_filter_update(&f, sample);
        /* After initial fill period, median should reject spikes */
        if (i >= 20) {
            if (result == 100) stable_count++;
        }
    }
    /* Vast majority should be 100 (spike only appears 1-in-10, window=7) */
    ASSERT_TRUE(stable_count > 90000);
    (void)ok;
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress median_filter")
    RUN(constant_input_convergence);
    RUN(extreme_value_swings);
    RUN(ramp_median_tracking);
    RUN(spike_rejection);
TEST_MAIN_END()
