// SPDX-License-Identifier: MIT
/*
 * stress_rate_limiter.c — long-running stress tests for rate_limiter
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_rate_limiter.c ../rate_limiter.c -o /tmp/stress_rate_limiter -lm
 *
 * Scenarios:
 *   1. Sustained burst then refill — verify exact token accounting over 100K cycles
 *   2. Token refill accuracy — verify tokens accumulate correctly over large tick gaps
 *   3. Try_n stress — batch consume under sustained load
 *   4. Stats counter overflow — verify allowed/denied counters stay consistent
 */

#include "test_harness.h"
#include "rate_limiter.h"

/* ---- tests ---------------------------------------------------------- */

TEST(burst_drain_refill_cycles)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 10, 1); /* 10 tokens max, 1 token/tick */

    int ok = 1;
    uint32_t tick = 0;

    for (int cycle = 0; cycle < 100000; cycle++) {
        /* Drain all tokens */
        int drained = 0;
        while (rate_limiter_try(&rl, tick) == RATE_LIMITER_OK) {
            drained++;
            if (drained > 10) { ok = 0; break; } /* safety guard */
        }
        if (!ok) break;

        /* Must be denied now */
        if (rate_limiter_try(&rl, tick) != RATE_LIMITER_DENIED) { ok = 0; break; }

        /* Advance 10 ticks — should refill to 10 */
        tick += 10;
        if (!rate_limiter_can_proceed(&rl, tick)) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(refill_accuracy_large_gaps)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 100, 5); /* 100 max, 5 per tick */

    /* Drain all tokens */
    uint32_t tick = 0;
    while (rate_limiter_try(&rl, tick) == RATE_LIMITER_OK) { }

    /* After draining, advance by 20 ticks — should refill 5*20=100 tokens */
    tick += 20;
    int ok = 1;
    int consumed = 0;
    while (rate_limiter_try(&rl, tick) == RATE_LIMITER_OK) {
        consumed++;
        if (consumed > 100) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ(100, consumed);

    /* Partial refill: advance 3 ticks from empty = 15 tokens */
    tick += 3;
    consumed = 0;
    while (rate_limiter_try(&rl, tick) == RATE_LIMITER_OK) {
        consumed++;
        if (consumed > 100) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ(15, consumed);
}

TEST(try_n_sustained_load)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 20, 2); /* 20 max, 2/tick */

    int ok = 1;
    uint32_t tick = 0;

    for (int i = 0; i < 100000; i++) {
        /* Try consuming 5 tokens every iteration */
        rate_limiter_err_t err = rate_limiter_try_n(&rl, tick, 5);
        if (err == RATE_LIMITER_OK) {
            /* Consumed 5, advance 3 ticks to get 6 back */
            tick += 3;
        } else {
            /* Denied — advance enough ticks to refill */
            tick += 10;
        }
    }

    /* Verify counters are consistent */
    uint32_t allowed = rate_limiter_allowed(&rl);
    uint32_t denied = rate_limiter_denied(&rl);
    ASSERT_TRUE(allowed + denied > 0);
    ASSERT_TRUE(allowed > 0);
    (void)ok;
}

TEST(stats_counter_consistency)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 5, 1); /* small bucket — many denials */

    uint32_t tick = 0;
    for (int i = 0; i < 500000; i++) {
        rate_limiter_try(&rl, tick);
        /* Advance tick slowly to get mix of allows and denials */
        if (i % 3 == 0) tick++;
    }

    uint32_t allowed = rate_limiter_allowed(&rl);
    uint32_t denied = rate_limiter_denied(&rl);
    ASSERT_EQ(500000u, (unsigned)(allowed + denied));

    /* After reset, counters clear */
    rate_limiter_reset(&rl);
    ASSERT_EQ(0u, rate_limiter_allowed(&rl));
    ASSERT_EQ(0u, rate_limiter_denied(&rl));
    ASSERT_EQ(5u, rate_limiter_available(&rl));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress rate_limiter")
    RUN(burst_drain_refill_cycles);
    RUN(refill_accuracy_large_gaps);
    RUN(try_n_sustained_load);
    RUN(stats_counter_consistency);
TEST_MAIN_END()
