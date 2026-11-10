// SPDX-License-Identifier: MIT
/*
 * test_destroy_tick_soft_timer.c -- a tick landing inside destroy
 *
 * destroy clears a live slot with memset, and callback sits before active. A
 * tick ISR that lands between the two calls NULL. This test puts a tick after
 * every byte memset clears, so each point an ISR could land is tried in turn,
 * and an ISR's rule holds: the tick runs to its end before destroy goes on.
 * A host thread cannot give that rule, and glibc's memset is too wide for a
 * real signal to land inside it, which is why this builds the wheel in here.
 */

#include <string.h>
#include <stdint.h>
#include <stddef.h>

static void *memset_with_ticks(void *dst, int value, size_t n);
#define memset memset_with_ticks
#include "../soft_timer.c"
#undef memset

#include "test_harness.h"

static soft_timer_mgr_t g_mgr;
static int              g_tick_inside;
static int              g_fired;
static long             g_ticks_landed;

static void *memset_with_ticks(void *dst, int value, size_t n)
{
    uint8_t *bytes = dst;

    for (size_t i = 0; i < n; i++) {
        bytes[i] = (uint8_t)value;
        if (g_tick_inside != 0) {
            soft_timer_tick(&g_mgr);
            g_ticks_landed++;
        }
    }
    return dst;
}

static void on_fire(void *ctx)
{
    (void)ctx;
    g_fired++;
}

TEST(test_tick_inside_destroy_never_calls_a_cleared_callback)
{
    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_init(&g_mgr));

    int id = soft_timer_create(&g_mgr, 1u, SOFT_TIMER_REPEAT, on_fire, NULL);

    ASSERT_EQ(0, id);
    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_start(&g_mgr, id));

    g_tick_inside = 1;
    ASSERT_EQ(SOFT_TIMER_OK, soft_timer_destroy(&g_mgr, id));
    g_tick_inside = 0;

    ASSERT_EQ((long)sizeof(soft_timer_entry_t), g_ticks_landed);
    ASSERT_EQ(0, g_fired);
    ASSERT_TRUE(!soft_timer_is_active(&g_mgr, id));
}

TEST_MAIN("soft_timer destroy beside a tick")
    RUN(test_tick_inside_destroy_never_calls_a_cleared_callback);
TEST_MAIN_END()
