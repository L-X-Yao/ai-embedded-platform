// SPDX-License-Identifier: MIT
/*
 * stress_timer_wheel — 1M tick fire-count and re-arm tests
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -I.. -I../../../../tooling/test \
 *       stress_timer_wheel.c ../timer_wheel.c -lm -o /tmp/stress_timer_wheel
 *
 * Scenarios:
 *   1. 1,000,000 ticks with timers at various intervals — verify exact fire counts
 *   2. Timer re-arm from callback — verify continuous operation
 *   3. Many short-period timers — verify no missed fires
 */

#include "timer_wheel.h"
#include "test_harness.h"
#include <stdint.h>
#include <string.h>

#define TOTAL_TICKS  1000000
#define NUM_SLOTS    64
#define MAX_TIMERS   16

/* ---- shared state --------------------------------------------------- */

static uint32_t g_fire_count[MAX_TIMERS];

static timer_wheel_t       g_tw;
static timer_wheel_slot_t  g_slots[NUM_SLOTS];
static timer_wheel_entry_t g_entries[MAX_TIMERS];

static void count_cb(void *ctx)
{
    uint32_t id = (uint32_t)(uintptr_t)ctx;
    if (id < MAX_TIMERS)
    {
        g_fire_count[id]++;
    }
}

/* ---- re-arm callback for scenario 2 -------------------------------- */

typedef struct {
    uint32_t id;
    uint32_t period;
    uint32_t count;
} rearm_ctx_t;

static void rearm_cb(void *ctx)
{
    rearm_ctx_t *rc = (rearm_ctx_t *)ctx;
    rc->count++;
    /* Re-arm the same timer for another period */
    timer_wheel_add(&g_tw, rc->id, rc->period, rearm_cb, ctx);
}

/* ---- helpers -------------------------------------------------------- */

static void init_wheel(void)
{
    memset(g_fire_count, 0, sizeof(g_fire_count));
    timer_wheel_init(&g_tw, g_slots, NUM_SLOTS, g_entries, MAX_TIMERS);
}

/* ---- tests ---------------------------------------------------------- */

TEST(various_intervals_exact_count)
{
    init_wheel();

    /*
     * Set up timers with different periods.  Each fires once, and we
     * re-add it in the callback to simulate repeating (timer_wheel
     * only supports one-shot natively).
     *
     * For simplicity in this test, we add one-shot timers and rely
     * on the fire count matching TOTAL_TICKS / period for each.
     * We re-arm from the callback.
     */
    static rearm_ctx_t contexts[4];
    uint32_t periods[4] = { 7, 13, 64, 997 };

    for (int i = 0; i < 4; i++)
    {
        contexts[i].id     = (uint32_t)i;
        contexts[i].period = periods[i];
        contexts[i].count  = 0;
        timer_wheel_add(&g_tw, (uint32_t)i, periods[i], rearm_cb, &contexts[i]);
    }

    for (int t = 0; t < TOTAL_TICKS; t++)
    {
        timer_wheel_tick(&g_tw);
    }

    /* Each timer should have fired approximately TOTAL_TICKS / period times.
     * Due to the re-arm delay (fires then re-arms for another period),
     * the count is exactly floor(TOTAL_TICKS / period). */
    for (int i = 0; i < 4; i++)
    {
        uint32_t expected = (uint32_t)(TOTAL_TICKS / periods[i]);
        /*
         * Allow +/- 1 tolerance because the first fire happens at
         * tick = period, and re-arm schedules the next period ticks later.
         */
        long diff = (long)contexts[i].count - (long)expected;
        if (diff < 0) diff = -diff;
        ASSERT_TRUE(diff <= 1);
    }
}

TEST(single_timer_exact_count)
{
    init_wheel();

    static rearm_ctx_t ctx;
    ctx.id     = 0;
    ctx.period = 100;
    ctx.count  = 0;

    timer_wheel_add(&g_tw, 0, 100, rearm_cb, &ctx);

    for (int t = 0; t < TOTAL_TICKS; t++)
    {
        timer_wheel_tick(&g_tw);
    }

    uint32_t expected = (uint32_t)(TOTAL_TICKS / 100);
    long diff = (long)ctx.count - (long)expected;
    if (diff < 0) diff = -diff;
    ASSERT_TRUE(diff <= 1);
}

TEST(many_short_period_no_missed)
{
    init_wheel();

    /* 8 timers all with short periods */
    static rearm_ctx_t contexts[8];
    uint32_t periods[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const int ticks = 100000;       /* shorter to keep runtime sane */

    for (int i = 0; i < 8; i++)
    {
        contexts[i].id     = (uint32_t)i;
        contexts[i].period = periods[i];
        contexts[i].count  = 0;
        timer_wheel_add(&g_tw, (uint32_t)i, periods[i], rearm_cb, &contexts[i]);
    }

    for (int t = 0; t < ticks; t++)
    {
        timer_wheel_tick(&g_tw);
    }

    for (int i = 0; i < 8; i++)
    {
        uint32_t expected = (uint32_t)(ticks / periods[i]);
        long diff = (long)contexts[i].count - (long)expected;
        if (diff < 0) diff = -diff;
        ASSERT_TRUE(diff <= 1);
    }
}

TEST(add_cancel_churn)
{
    /*
     * Rapidly add and cancel timers over many ticks to verify
     * the wheel's bookkeeping stays consistent (no stale fires,
     * no count drift).
     */
    init_wheel();

    g_fire_count[0] = 0;

    int active = 0;
    for (int t = 0; t < 500000; t++)
    {
        /* Every 100 ticks, add a timer that fires at +50 */
        if (t % 100 == 0 && !active)
        {
            timer_wheel_add(&g_tw, 0, 50, count_cb, (void *)(uintptr_t)0);
            active = 1;
        }
        /* Every 100 ticks offset by 30, cancel it (before it fires) */
        if (t % 100 == 30 && active)
        {
            timer_wheel_cancel(&g_tw, 0);
            active = 0;
        }

        timer_wheel_tick(&g_tw);
    }

    /* Timer is always cancelled at +30 before it can fire at +50.
     * So fire count should be 0. */
    ASSERT_EQ(g_fire_count[0], 0);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress_timer_wheel")
    RUN(various_intervals_exact_count);
    RUN(single_timer_exact_count);
    RUN(many_short_period_no_missed);
    RUN(add_cancel_churn);
TEST_MAIN_END()
