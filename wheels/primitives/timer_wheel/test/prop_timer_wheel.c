// SPDX-License-Identifier: MIT
/*
 * prop_timer_wheel.c — property-based tests for timer_wheel
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_timer_wheel.c ../timer_wheel.c -o prop_timer_wheel
 */

#include "test_harness.h"
#include "property_test.h"
#include "../timer_wheel.h"
#include <string.h>

#define NUM_SLOTS   16
#define MAX_TIMERS  8

/* ---- callback tracker ---------------------------------------------- */

static int g_fired[MAX_TIMERS];
static int g_fire_count;

static void fire_cb(void *ctx) {
    int id = (int)(intptr_t)ctx;
    if (id >= 0 && id < MAX_TIMERS) {
        g_fired[id]++;
    }
    g_fire_count++;
}

static void reset_fire_state(void) {
    memset(g_fired, 0, sizeof(g_fired));
    g_fire_count = 0;
}

/* Property: add-expire roundtrip — timer fires after exact tick count */
PROPERTY(add_expire_roundtrip, 5000) {
    reset_fire_state();
    timer_wheel_slot_t slots[NUM_SLOTS];
    timer_wheel_entry_t timers[MAX_TIMERS];
    timer_wheel_t tw;
    timer_wheel_init(&tw, slots, NUM_SLOTS, timers, MAX_TIMERS);

    uint32_t delay = (uint32_t)ARBI_RANGE(seed, 1, 200);
    timer_wheel_err_t err = timer_wheel_add(&tw, 0, delay, fire_cb, (void *)(intptr_t)0);
    PROP_ASSERT_EQ(err, TIMER_WHEEL_OK);

    /* Tick delay-1 times: should not fire */
    for (uint32_t i = 0; i < delay - 1; i++) {
        timer_wheel_tick(&tw);
    }
    PROP_ASSERT_EQ(g_fired[0], 0);

    /* One more tick: should fire */
    timer_wheel_tick(&tw);
    PROP_ASSERT_EQ(g_fired[0], 1);
}

/* Property: no spurious fires — only scheduled timers fire */
PROPERTY(no_spurious_fires, 5000) {
    reset_fire_state();
    timer_wheel_slot_t slots[NUM_SLOTS];
    timer_wheel_entry_t timers[MAX_TIMERS];
    timer_wheel_t tw;
    timer_wheel_init(&tw, slots, NUM_SLOTS, timers, MAX_TIMERS);

    /* Add timer at far future */
    uint32_t delay = (uint32_t)ARBI_RANGE(seed, 100, 500);
    timer_wheel_add(&tw, 0, delay, fire_cb, (void *)(intptr_t)0);

    /* Tick fewer than delay times */
    uint32_t ticks = (uint32_t)ARBI_RANGE(seed, 1, delay - 1);
    for (uint32_t i = 0; i < ticks; i++) {
        timer_wheel_tick(&tw);
    }

    PROP_ASSERT_EQ(g_fire_count, 0);
}

/* Property: cancel prevents firing */
PROPERTY(cancel_prevents_fire, 5000) {
    reset_fire_state();
    timer_wheel_slot_t slots[NUM_SLOTS];
    timer_wheel_entry_t timers[MAX_TIMERS];
    timer_wheel_t tw;
    timer_wheel_init(&tw, slots, NUM_SLOTS, timers, MAX_TIMERS);

    uint32_t delay = (uint32_t)ARBI_RANGE(seed, 5, 100);
    timer_wheel_add(&tw, 0, delay, fire_cb, (void *)(intptr_t)0);

    /* Cancel before it fires */
    uint32_t cancel_at = (uint32_t)ARBI_RANGE(seed, 0, delay - 1);
    for (uint32_t i = 0; i < cancel_at; i++) {
        timer_wheel_tick(&tw);
    }
    timer_wheel_err_t err = timer_wheel_cancel(&tw, 0);
    PROP_ASSERT_EQ(err, TIMER_WHEEL_OK);

    /* Tick past the original delay */
    for (uint32_t i = cancel_at; i < delay + 10; i++) {
        timer_wheel_tick(&tw);
    }

    PROP_ASSERT_EQ(g_fired[0], 0);
}

/* Property: multiple timers fire independently */
PROPERTY(multiple_timers, 5000) {
    reset_fire_state();
    timer_wheel_slot_t slots[NUM_SLOTS];
    timer_wheel_entry_t timers[MAX_TIMERS];
    timer_wheel_t tw;
    timer_wheel_init(&tw, slots, NUM_SLOTS, timers, MAX_TIMERS);

    unsigned n = (unsigned)ARBI_RANGE(seed, 2, MAX_TIMERS);
    uint32_t max_delay = 0;
    for (unsigned i = 0; i < n; i++) {
        uint32_t delay = (uint32_t)ARBI_RANGE(seed, 1, 100);
        if (delay > max_delay) max_delay = delay;
        timer_wheel_add(&tw, (uint32_t)i, delay, fire_cb,
                         (void *)(intptr_t)i);
    }

    /* Tick past all delays */
    for (uint32_t t = 0; t <= max_delay; t++) {
        timer_wheel_tick(&tw);
    }

    /* Every timer should have fired exactly once */
    for (unsigned i = 0; i < n; i++) {
        PROP_ASSERT_EQ(g_fired[i], 1);
    }
}

TEST_MAIN("timer_wheel properties") {
    RUN(add_expire_roundtrip);
    RUN(no_spurious_fires);
    RUN(cancel_prevents_fire);
    RUN(multiple_timers);
}
TEST_MAIN_END()
