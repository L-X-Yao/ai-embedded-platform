// SPDX-License-Identifier: MIT
/*
 * stress_timeout — 32-bit tick wraparound and edge-case deadline tests
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -I.. -I../../../../tooling/test \
 *       stress_timeout.c ../timeout.c -lm -o /tmp/stress_timeout
 *
 * Scenarios:
 *   1. Start near UINT32_MAX, tick past wrap — expired detection works
 *   2. Duration = UINT32_MAX — longest possible timeout
 *   3. Remaining / elapsed across wrap boundary
 *   4. Many set/check cycles with wrapped ticks
 */

#include "timeout.h"
#include "test_harness.h"
#include <stdint.h>

/* ---- tests ---------------------------------------------------------- */

TEST(wrap_basic_expired)
{
    timeout_t t;
    uint32_t start = UINT32_MAX - 10u;
    timeout_set(&t, start, 20);

    /* Before expiry: start+19 = UINT32_MAX+9 = 8 (wrapped) */
    ASSERT_FALSE(timeout_expired(&t, start + 19));

    /* Exactly at expiry: start+20 */
    ASSERT_TRUE(timeout_expired(&t, start + 20));

    /* Well past expiry */
    ASSERT_TRUE(timeout_expired(&t, start + 1000));
}

TEST(wrap_remaining_elapsed)
{
    timeout_t t;
    uint32_t start = UINT32_MAX - 50u;
    timeout_set(&t, start, 100);

    /* Before expiry — check remaining/elapsed */
    uint32_t now = start + 30;
    ASSERT_EQ(timeout_elapsed(&t, now), 30);
    ASSERT_EQ(timeout_remaining(&t, now), 70);

    /* Past the 32-bit wrap but before expiry */
    now = start + 80;                   /* wrapped: UINT32_MAX - 50 + 80 = 29 */
    ASSERT_EQ(timeout_elapsed(&t, now), 80);
    ASSERT_EQ(timeout_remaining(&t, now), 20);

    /* Past expiry */
    now = start + 150;
    ASSERT_TRUE(timeout_expired(&t, now));
    ASSERT_EQ(timeout_remaining(&t, now), 0);
    ASSERT_EQ(timeout_elapsed(&t, now), 150);
}

TEST(duration_max)
{
    /* Longest possible timeout: UINT32_MAX ticks */
    timeout_t t;
    timeout_set(&t, 0, UINT32_MAX);

    /* Not expired at UINT32_MAX - 1 elapsed */
    ASSERT_FALSE(timeout_expired(&t, UINT32_MAX - 1));

    /* Expired at exactly UINT32_MAX elapsed */
    ASSERT_TRUE(timeout_expired(&t, UINT32_MAX));
}

TEST(restart_across_wrap)
{
    timeout_t t;
    uint32_t start = UINT32_MAX - 5u;
    timeout_set(&t, start, 20);

    /* Tick past wrap, restart */
    uint32_t now = start + 10;          /* wrapped to ~4 */
    timeout_restart(&t, now);

    /* Should not be expired for another 20 ticks */
    ASSERT_FALSE(timeout_expired(&t, now + 19));
    ASSERT_TRUE(timeout_expired(&t, now + 20));
}

TEST(sweep_all_start_offsets)
{
    /*
     * Sweep many start values near UINT32_MAX, each with a small
     * duration.  Verifies the (now - start) >= duration arithmetic
     * holds for all boundary positions.
     */
    int ok = 1;
    const uint32_t duration = 50;

    for (uint32_t off = 0; off < 200; off++)
    {
        uint32_t start = UINT32_MAX - 100u + off;
        timeout_t t;
        timeout_set(&t, start, duration);

        /* One tick before expiry */
        if (timeout_expired(&t, start + duration - 1))
        {
            ok = 0;
            break;
        }
        /* Exactly at expiry */
        if (!timeout_expired(&t, start + duration))
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
}

TEST(extend_across_wrap)
{
    timeout_t t;
    uint32_t start = UINT32_MAX - 10u;
    timeout_set(&t, start, 15);

    /* Extend by 10 — new duration is 25 */
    timeout_extend(&t, 10);

    ASSERT_FALSE(timeout_expired(&t, start + 24));
    ASSERT_TRUE(timeout_expired(&t, start + 25));
}

TEST(extend_saturates)
{
    /* Extending past UINT32_MAX should saturate, not wrap to small */
    timeout_t t;
    timeout_set(&t, 0, UINT32_MAX - 5u);
    timeout_extend(&t, 100);

    /* Duration should be clamped to UINT32_MAX */
    ASSERT_FALSE(timeout_expired(&t, UINT32_MAX - 1));
    ASSERT_TRUE(timeout_expired(&t, UINT32_MAX));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress_timeout")
    RUN(wrap_basic_expired);
    RUN(wrap_remaining_elapsed);
    RUN(duration_max);
    RUN(restart_across_wrap);
    RUN(sweep_all_start_offsets);
    RUN(extend_across_wrap);
    RUN(extend_saturates);
TEST_MAIN_END()
