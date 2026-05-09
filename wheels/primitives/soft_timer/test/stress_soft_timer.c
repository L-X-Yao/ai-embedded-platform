// SPDX-License-Identifier: MIT
/*
 * stress_soft_timer — 32-bit tick wrap and long-running fire-count tests
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -I.. -I../../../../tooling/harness \
 *       stress_soft_timer.c ../soft_timer.c -lm -o /tmp/stress_soft_timer
 *
 * Scenarios:
 *   1. Tick from UINT32_MAX-100 to UINT32_MAX+100 — timers must still fire
 *   2. Repeating timer over 1,000,000 ticks — fire count must match
 *   3. Multiple timers at different periods — verify exact fire counts
 */

#include "soft_timer.h"
#include "test_harness.h"
#include <stdint.h>

/* ---- shared counters ------------------------------------------------ */

static volatile uint32_t g_fire_a;
static volatile uint32_t g_fire_b;
static volatile uint32_t g_fire_c;

static void cb_a(void *ctx) { (void)ctx; g_fire_a++; }
static void cb_b(void *ctx) { (void)ctx; g_fire_b++; }
static void cb_c(void *ctx) { (void)ctx; g_fire_c++; }

/* ---- tests ---------------------------------------------------------- */

TEST(tick_wrap_fires_correctly)
{
    /*
     * soft_timer uses a countdown (remaining--) rather than absolute tick
     * comparison, so it is inherently immune to tick_count wrap.  We verify
     * that by running 200 ticks around the internal tick_count overflow
     * boundary, with a period-10 repeating timer.
     */
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    /* Pre-wind tick_count close to UINT32_MAX */
    mgr.tick_count = UINT32_MAX - 100u;

    g_fire_a = 0;
    int id = soft_timer_create(&mgr, 10, SOFT_TIMER_REPEAT, cb_a, NULL);
    ASSERT_TRUE(id >= 0);
    soft_timer_start(&mgr, id);

    /* Tick 200 times — crosses UINT32_MAX boundary */
    for (int i = 0; i < 200; i++)
    {
        soft_timer_tick(&mgr);
    }

    /* period=10, 200 ticks => 20 fires */
    ASSERT_EQ(g_fire_a, 20);
}

TEST(repeat_million_ticks)
{
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    g_fire_a = 0;
    int id = soft_timer_create(&mgr, 100, SOFT_TIMER_REPEAT, cb_a, NULL);
    ASSERT_TRUE(id >= 0);
    soft_timer_start(&mgr, id);

    for (int i = 0; i < 1000000; i++)
    {
        soft_timer_tick(&mgr);
    }

    /* 1,000,000 / 100 = 10,000 fires */
    ASSERT_EQ(g_fire_a, 10000);
}

TEST(multiple_timers_exact_counts)
{
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    g_fire_a = 0;
    g_fire_b = 0;
    g_fire_c = 0;

    int id_a = soft_timer_create(&mgr, 7,   SOFT_TIMER_REPEAT, cb_a, NULL);
    int id_b = soft_timer_create(&mgr, 13,  SOFT_TIMER_REPEAT, cb_b, NULL);
    int id_c = soft_timer_create(&mgr, 997, SOFT_TIMER_REPEAT, cb_c, NULL);

    ASSERT_TRUE(id_a >= 0);
    ASSERT_TRUE(id_b >= 0);
    ASSERT_TRUE(id_c >= 0);

    soft_timer_start(&mgr, id_a);
    soft_timer_start(&mgr, id_b);
    soft_timer_start(&mgr, id_c);

    const int total_ticks = 500000;
    for (int i = 0; i < total_ticks; i++)
    {
        soft_timer_tick(&mgr);
    }

    /* integer division: total_ticks / period */
    ASSERT_EQ(g_fire_a, (uint32_t)(total_ticks / 7));
    ASSERT_EQ(g_fire_b, (uint32_t)(total_ticks / 13));
    ASSERT_EQ(g_fire_c, (uint32_t)(total_ticks / 997));
}

TEST(one_shot_fires_once_after_wrap)
{
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);
    mgr.tick_count = UINT32_MAX - 5u;

    g_fire_a = 0;
    int id = soft_timer_create(&mgr, 50, SOFT_TIMER_ONE_SHOT, cb_a, NULL);
    ASSERT_TRUE(id >= 0);
    soft_timer_start(&mgr, id);

    for (int i = 0; i < 200; i++)
    {
        soft_timer_tick(&mgr);
    }

    /* One-shot must fire exactly once */
    ASSERT_EQ(g_fire_a, 1);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress_soft_timer")
    RUN(tick_wrap_fires_correctly);
    RUN(repeat_million_ticks);
    RUN(multiple_timers_exact_counts);
    RUN(one_shot_fires_once_after_wrap);
TEST_MAIN_END()
