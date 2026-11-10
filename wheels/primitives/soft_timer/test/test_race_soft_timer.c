// SPDX-License-Identifier: MIT
/*
 * test_race_soft_timer.c -- polling beside a live tick
 *
 * The header lets the main loop use the timers while tick runs in an ISR. A
 * host thread stands in for that ISR. This target builds with -O2 -flto, so
 * the compiler sees through the calls: a field the tick writes that is not
 * volatile gets read once, and a poll never ends.
 *
 * What this cannot show. A pass is evidence, not proof: it covers the
 * interleavings that happened to occur. A thread is not an ISR: main can run
 * while it is mid-tick, which the header forbids, so nothing here may destroy a
 * timer beside it (test_destroy_tick_soft_timer.c covers that with a hook). x86
 * keeps stores in order, so it says nothing about a weakly ordered core; run on
 * one, this test needs SOFT_TIMER_BARRIER defined as a real fence.
 */

#include "test_harness.h"
#include <pthread.h>
#include "soft_timer.h"

#define POLL_LIMIT 2000000000L /* polls before a wait counts as stuck */
#define TICKS      100000u

static soft_timer_mgr_t g_mgr;
static int              g_stop;
static int              g_fired;

static void on_fire(void *ctx)
{
    (void)ctx;
    __atomic_add_fetch(&g_fired, 1, __ATOMIC_RELAXED);
}

static void *ticker(void *arg)
{
    (void)arg;
    while (__atomic_load_n(&g_stop, __ATOMIC_RELAXED) == 0) {
        soft_timer_tick(&g_mgr);
    }
    return NULL;
}

TEST(test_poll_of_get_tick_sees_the_ticks)
{
    pthread_t tick;
    long polls = 0;

    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_init(&g_mgr));
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_create(&tick, NULL, ticker, NULL));

    while (soft_timer_get_tick(&g_mgr) < TICKS && polls < POLL_LIMIT) {
        polls++;
    }
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_join(tick, NULL));
    ASSERT_TRUE(polls < POLL_LIMIT);
}

TEST(test_poll_of_is_active_sees_a_one_shot_fire)
{
    pthread_t tick;
    long polls = 0;

    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_init(&g_mgr));
    ASSERT_EQ(0, soft_timer_create(&g_mgr, TICKS, SOFT_TIMER_ONE_SHOT, on_fire, NULL));
    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_start(&g_mgr, 0));
    __atomic_store_n(&g_fired, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_create(&tick, NULL, ticker, NULL));

    while (soft_timer_is_active(&g_mgr, 0) && polls < POLL_LIMIT) {
        polls++;
    }
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_join(tick, NULL));
    ASSERT_TRUE(polls < POLL_LIMIT);
    ASSERT_EQ(1, __atomic_load_n(&g_fired, __ATOMIC_RELAXED));
}

TEST_MAIN("soft_timer race")
    RUN(test_poll_of_get_tick_sees_the_ticks);
    RUN(test_poll_of_is_active_sees_a_one_shot_fire);
TEST_MAIN_END()
