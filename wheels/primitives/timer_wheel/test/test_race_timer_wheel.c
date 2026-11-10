// SPDX-License-Identifier: MIT
/*
 * test_race_timer_wheel.c -- a retry of add beside a live tick
 *
 * add returns TIMER_WHEEL_ERR_FULL while an id is in use, so a caller can wait
 * for a timer to fire by retrying it. A host thread stands in for the tick ISR
 * and stops once the timer has fired, because add itself must not run beside
 * a tick. This target builds with -O3 -flto, so the compiler sees through the
 * calls: an active field that is not volatile gets read once, and the retry
 * never ends. With GCC 13, -O2 -flto did not show the fault; -O3 -flto does.
 *
 * What this cannot show. It covers the wait only. add beside a running tick is
 * not safe, and the header says to mask the tick's interrupt around it.
 */

#include "test_harness.h"
#include <pthread.h>
#include "timer_wheel.h"

#define TICKS      100000u
#define POLL_LIMIT 2000000000L /* retries before a wait counts as stuck */

static timer_wheel_t       g_wheel;
static timer_wheel_slot_t  g_slots[8];
static timer_wheel_entry_t g_timers[1];
static int                 g_fired;

static void on_fire(void *ctx)
{
    (void)ctx;
    __atomic_store_n(&g_fired, 1, __ATOMIC_RELEASE);
}

static void *ticker(void *arg)
{
    (void)arg;
    while (__atomic_load_n(&g_fired, __ATOMIC_ACQUIRE) == 0) {
        timer_wheel_tick(&g_wheel);
    }
    return NULL;
}

TEST(test_retry_of_add_sees_the_timer_fire)
{
    pthread_t tick;
    long polls = 0;
    timer_wheel_err_t err;

    ASSERT_EQ(TIMER_WHEEL_OK, timer_wheel_init(&g_wheel, g_slots, 8, g_timers, 1));
    ASSERT_EQ(TIMER_WHEEL_OK, timer_wheel_add(&g_wheel, 0, TICKS, on_fire, NULL));
    ASSERT_EQ(0, pthread_create(&tick, NULL, ticker, NULL));

    do {
        err = timer_wheel_add(&g_wheel, 0, TICKS, on_fire, NULL);
        polls++;
    } while (err == TIMER_WHEEL_ERR_FULL && polls < POLL_LIMIT);

    if (err == TIMER_WHEEL_ERR_FULL) {
        __atomic_store_n(&g_fired, 1, __ATOMIC_RELEASE); /* let the ticker go */
    }
    ASSERT_EQ(0, pthread_join(tick, NULL));
    ASSERT_EQ(TIMER_WHEEL_OK, err);
}

TEST_MAIN("timer_wheel race")
    RUN(test_retry_of_add_sees_the_timer_fire);
TEST_MAIN_END()
