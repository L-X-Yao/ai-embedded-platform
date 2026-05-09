// SPDX-License-Identifier: MIT
/*
 * stress_led_pattern.c — long-running stress tests for led_pattern
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_led_pattern.c ../led_pattern.c -o /tmp/stress_led_pattern -lm
 *
 * Scenarios:
 *   1. Long looping pattern — 1M ticks, verify periodic behavior
 *   2. One-shot completion — many restarts, verify finished exactly at expected tick
 *   3. Multi-step timing — verify cumulative timing stays exact over 100K loops
 *   4. Restart stress — rapid restart mid-pattern, verify clean reset
 */

#include "test_harness.h"
#include "led_pattern.h"

/* ---- tests ---------------------------------------------------------- */

TEST(looping_pattern_million_ticks)
{
    /* Simple blink: 10 on, 10 off — period = 20 ticks */
    static const led_step_t steps[] = {
        { .on_ticks = 10, .off_ticks = 10 },
    };
    led_pattern_cfg_t cfg = {
        .steps      = steps,
        .step_count = 1,
        .loop       = true,
    };
    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    int ok = 1;
    for (int tick = 0; tick < 1000000; tick++) {
        bool on = led_pattern_tick(&p);
        /* Tick within step: 0-9 = on, 10-19 = off (mod 20) */
        int phase = tick % 20;
        bool expected = (phase < 10);
        if (on != expected) { ok = 0; break; }
        if (led_pattern_finished(&p)) { ok = 0; break; } /* looping never finishes */
    }
    ASSERT_TRUE(ok);
}

TEST(one_shot_precise_completion)
{
    /* 3-step one-shot: (5on,5off), (3on,3off), (2on,2off) = 20 ticks total */
    static const led_step_t steps[] = {
        { .on_ticks = 5, .off_ticks = 5 },
        { .on_ticks = 3, .off_ticks = 3 },
        { .on_ticks = 2, .off_ticks = 2 },
    };
    led_pattern_cfg_t cfg = {
        .steps      = steps,
        .step_count = 3,
        .loop       = false,
    };
    led_pattern_t p;

    int ok = 1;
    for (int restart = 0; restart < 50000; restart++) {
        led_pattern_init(&p, &cfg);
        int tick = 0;
        while (!led_pattern_finished(&p) && tick < 100) {
            led_pattern_tick(&p);
            tick++;
        }
        /* Pattern should complete at exactly tick 20 */
        if (tick != 20) { ok = 0; break; }
        if (!led_pattern_finished(&p)) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(multi_step_timing_accuracy)
{
    /* 4-step looping pattern: (2on,3off), (1on,4off), (3on,2off), (4on,1off) */
    /* Period = (2+3) + (1+4) + (3+2) + (4+1) = 5+5+5+5 = 20 ticks */
    static const led_step_t steps[] = {
        { .on_ticks = 2, .off_ticks = 3 },
        { .on_ticks = 1, .off_ticks = 4 },
        { .on_ticks = 3, .off_ticks = 2 },
        { .on_ticks = 4, .off_ticks = 1 },
    };
    led_pattern_cfg_t cfg = {
        .steps      = steps,
        .step_count = 4,
        .loop       = true,
    };
    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    /* Expected on/off pattern per period:
     * tick 0-1: on, 2-4: off, 5: on, 6-9: off, 10-12: on, 13-14: off, 15-18: on, 19: off */
    static const bool expected_pattern[20] = {
        1,1,0,0,0, 1,0,0,0,0, 1,1,1,0,0, 1,1,1,1,0
    };

    int ok = 1;
    for (int tick = 0; tick < 100000; tick++) {
        bool on = led_pattern_tick(&p);
        bool expected = expected_pattern[tick % 20];
        if (on != expected) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(restart_mid_pattern_stress)
{
    static const led_step_t steps[] = {
        { .on_ticks = 10, .off_ticks = 10 },
        { .on_ticks = 5,  .off_ticks = 5  },
    };
    led_pattern_cfg_t cfg = {
        .steps      = steps,
        .step_count = 2,
        .loop       = true,
    };
    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        /* Tick a few times into the pattern */
        for (int t = 0; t < (i % 7) + 1; t++) {
            led_pattern_tick(&p);
        }
        /* Restart — must reset cleanly */
        led_pattern_restart(&p);

        /* After restart, first tick should be on (step 0, on phase) */
        bool first = led_pattern_tick(&p);
        if (!first) { ok = 0; break; }
        if (led_pattern_finished(&p)) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress led_pattern")
    RUN(looping_pattern_million_ticks);
    RUN(one_shot_precise_completion);
    RUN(multi_step_timing_accuracy);
    RUN(restart_mid_pattern_stress);
TEST_MAIN_END()
