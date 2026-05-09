/*
 * prop_soft_timer.c — property-based tests for soft_timer
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_soft_timer.c ../soft_timer.c -o prop_soft_timer
 */

#include "test_harness.h"
#include "property_test.h"
#include "../soft_timer.h"

static int g_fire_count;
static void fire_cb(void *ctx) { (void)ctx; g_fire_count++; }

/* Property: tick counter is monotonically non-decreasing */
PROPERTY(tick_monotonic, 10000) {
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    uint32_t prev = soft_timer_get_tick(&mgr);
    unsigned ticks = (unsigned)ARBI_RANGE(seed, 1, 200);
    for (unsigned i = 0; i < ticks; i++) {
        soft_timer_tick(&mgr);
        uint32_t cur = soft_timer_get_tick(&mgr);
        PROP_ASSERT(cur > prev);
        prev = cur;
    }
}

/* Property: one-shot timer fires exactly once after configured period */
PROPERTY(oneshot_fires_at_period, 5000) {
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    uint32_t period = (uint32_t)ARBI_RANGE(seed, 1, 100);
    g_fire_count = 0;
    int id = soft_timer_create(&mgr, period, SOFT_TIMER_ONE_SHOT, fire_cb, NULL);
    PROP_ASSERT(id >= 0);
    soft_timer_start(&mgr, id);

    /* Tick period-1 times: should NOT have fired */
    for (uint32_t i = 0; i < period - 1; i++) {
        soft_timer_tick(&mgr);
    }
    PROP_ASSERT_EQ(g_fire_count, 0);

    /* One more tick: should fire */
    soft_timer_tick(&mgr);
    PROP_ASSERT_EQ(g_fire_count, 1);

    /* Additional ticks: should NOT fire again (one-shot) */
    for (int i = 0; i < 10; i++) {
        soft_timer_tick(&mgr);
    }
    PROP_ASSERT_EQ(g_fire_count, 1);

    soft_timer_destroy(&mgr, id);
}

/* Property: repeating timer fires multiple times at correct intervals */
PROPERTY(repeat_fires_periodically, 5000) {
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    uint32_t period = (uint32_t)ARBI_RANGE(seed, 1, 50);
    unsigned repeats = (unsigned)ARBI_RANGE(seed, 2, 5);
    g_fire_count = 0;

    int id = soft_timer_create(&mgr, period, SOFT_TIMER_REPEAT, fire_cb, NULL);
    PROP_ASSERT(id >= 0);
    soft_timer_start(&mgr, id);

    uint32_t total_ticks = period * repeats;
    for (uint32_t i = 0; i < total_ticks; i++) {
        soft_timer_tick(&mgr);
    }

    PROP_ASSERT_EQ(g_fire_count, (int)repeats);

    soft_timer_destroy(&mgr, id);
}

/* Property: stopped timer does not fire */
PROPERTY(stopped_no_fire, 5000) {
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    uint32_t period = (uint32_t)ARBI_RANGE(seed, 1, 50);
    g_fire_count = 0;

    int id = soft_timer_create(&mgr, period, SOFT_TIMER_REPEAT, fire_cb, NULL);
    PROP_ASSERT(id >= 0);
    soft_timer_start(&mgr, id);
    soft_timer_stop(&mgr, id);

    /* Tick well past the period */
    for (uint32_t i = 0; i < period * 3; i++) {
        soft_timer_tick(&mgr);
    }

    PROP_ASSERT_EQ(g_fire_count, 0);

    soft_timer_destroy(&mgr, id);
}

TEST_MAIN("soft_timer properties") {
    RUN(tick_monotonic);
    RUN(oneshot_fires_at_period);
    RUN(repeat_fires_periodically);
    RUN(stopped_no_fire);
}
TEST_MAIN_END()
