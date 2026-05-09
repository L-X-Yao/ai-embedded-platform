// SPDX-License-Identifier: MIT
/*
 * stress_debounce.c — long-running stress tests for debounce
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_debounce.c ../debounce.c -o /tmp/stress_debounce -lm
 *
 * Scenarios:
 *   1. Rapid toggle — 1M alternating true/false, stable state must not flicker
 *   2. Sustained press — confirm stable after threshold, no drift over 500K samples
 *   3. Noise burst then settle — repeated noise-then-clean patterns
 *   4. Edge counting — verify rising/falling edge counts over sustained operation
 */

#include "test_harness.h"
#include "debounce.h"

/* ---- tests ---------------------------------------------------------- */

TEST(rapid_toggle_no_flicker)
{
    debounce_t d;
    debounce_init(&d, 5, false);

    int stable_changes = 0;
    bool last_stable = false;

    /* Toggle raw every sample — stable should never change because
     * we never hold the same value for 5 consecutive samples */
    for (int i = 0; i < 1000000; i++) {
        bool raw = (i % 2 == 0);
        debounce_update(&d, raw);
        if (debounce_changed(&d)) stable_changes++;
        bool cur = debounce_get(&d);
        if (cur != last_stable && i > 0) {
            /* If stable changed, it must have been flagged */
        }
        last_stable = cur;
    }
    /* With alternating 0,1,0,1... and threshold=5, stable should never change */
    ASSERT_EQ(0, stable_changes);
    ASSERT_FALSE(debounce_get(&d));
}

TEST(sustained_press_no_drift)
{
    debounce_t d;
    debounce_init(&d, 10, false);

    /* Feed 10 trues to trigger stable=true */
    for (int i = 0; i < 10; i++) {
        debounce_update(&d, true);
    }
    ASSERT_TRUE(debounce_get(&d));

    /* Continue feeding true for 500K more samples — stable must stay true */
    int ok = 1;
    for (int i = 0; i < 500000; i++) {
        debounce_update(&d, true);
        if (!debounce_get(&d)) { ok = 0; break; }
        if (debounce_changed(&d)) { ok = 0; break; } /* no further changes */
    }
    ASSERT_TRUE(ok);
}

TEST(noise_burst_then_settle)
{
    debounce_t d;
    debounce_init(&d, 5, false);

    int ok = 1;
    for (int cycle = 0; cycle < 50000; cycle++) {
        /* Noise burst: 4 toggles (not enough to cross threshold) */
        for (int j = 0; j < 4; j++) {
            debounce_update(&d, (j % 2 == 0));
        }

        /* Settle to true: 5 consecutive true samples */
        for (int j = 0; j < 5; j++) {
            debounce_update(&d, true);
        }
        if (!debounce_get(&d)) { ok = 0; break; }

        /* Settle to false: 5 consecutive false samples */
        for (int j = 0; j < 5; j++) {
            debounce_update(&d, false);
        }
        if (debounce_get(&d)) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(edge_count_accuracy)
{
    debounce_t d;
    debounce_init(&d, 3, false);

    int rising_count = 0;
    int falling_count = 0;
    const int presses = 100000;

    for (int press = 0; press < presses; press++) {
        /* Press: 3 true samples */
        for (int j = 0; j < 3; j++) {
            debounce_update(&d, true);
            if (debounce_rising(&d)) rising_count++;
            if (debounce_falling(&d)) falling_count++;
        }
        /* Release: 3 false samples */
        for (int j = 0; j < 3; j++) {
            debounce_update(&d, false);
            if (debounce_rising(&d)) rising_count++;
            if (debounce_falling(&d)) falling_count++;
        }
    }
    /* Each press-release cycle should generate exactly 1 rising + 1 falling */
    ASSERT_EQ(presses, rising_count);
    ASSERT_EQ(presses, falling_count);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress debounce")
    RUN(rapid_toggle_no_flicker);
    RUN(sustained_press_no_drift);
    RUN(noise_burst_then_settle);
    RUN(edge_count_accuracy);
TEST_MAIN_END()
