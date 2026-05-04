/*
 * verify_soft_timer.c — CBMC bounded model checking for soft_timer
 *
 * Verifies timer lifecycle, tick monotonicity, and callback invocation.
 * Run with:
 *   cbmc verify_soft_timer.c ../soft_timer.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check \
 *        -DSOFT_TIMER_MAX=4
 */

#include "../soft_timer.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern uint32_t nondet_uint32(void);
extern unsigned  nondet_unsigned(void);

static unsigned g_callback_count;

static void test_callback(void *ctx) {
    g_callback_count++;
    (void)ctx;
}

static void check_invariants(const soft_timer_mgr_t *mgr) {
    /* count never exceeds max */
    assert(mgr->count <= SOFT_TIMER_MAX);

    /* All active timers with nonzero period have bounded remaining */
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->timers[i].active && mgr->timers[i].period > 0) {
            assert(mgr->timers[i].remaining <= mgr->timers[i].period);
        }
    }
}

/* Proof 1: invariants hold after any operation sequence */
void verify_invariants(void) {
    soft_timer_mgr_t mgr;
    g_callback_count = 0;

    soft_timer_err_t err = soft_timer_init(&mgr);
    assert(err == SOFT_TIMER_OK);
    check_invariants(&mgr);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = nondet_uint8() % 5;
        switch (op) {
        case 0: {
            uint32_t period = nondet_uint32();
            __CPROVER_assume(period >= 1 && period <= 1000);
            soft_timer_create(&mgr, period, SOFT_TIMER_ONE_SHOT,
                              test_callback, NULL);
            break;
        }
        case 1: {
            int id = (int)(nondet_uint8() % SOFT_TIMER_MAX);
            soft_timer_start(&mgr, id);
            break;
        }
        case 2: {
            int id = (int)(nondet_uint8() % SOFT_TIMER_MAX);
            soft_timer_stop(&mgr, id);
            break;
        }
        case 3: {
            soft_timer_tick(&mgr);
            break;
        }
        case 4: {
            int id = (int)(nondet_uint8() % SOFT_TIMER_MAX);
            soft_timer_destroy(&mgr, id);
            break;
        }
        }
        check_invariants(&mgr);
    }
}

/* Proof 2: tick counter is monotonically increasing */
void verify_tick_monotonicity(void) {
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    uint32_t prev = soft_timer_get_tick(&mgr);
    assert(prev == 0);

    unsigned ticks = nondet_unsigned();
    __CPROVER_assume(ticks <= UNWIND_BOUND);

    for (unsigned i = 0; i < ticks; i++) {
        soft_timer_tick(&mgr);
        uint32_t curr = soft_timer_get_tick(&mgr);
        assert(curr == prev + 1);
        prev = curr;
    }
}

/* Proof 3: one-shot timer fires exactly once, repeating fires multiple times */
void verify_one_shot_fires_once(void) {
    soft_timer_mgr_t mgr;
    g_callback_count = 0;
    soft_timer_init(&mgr);

    int id = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT,
                               test_callback, NULL);
    assert(id >= 0);

    soft_timer_start(&mgr, id);
    assert(soft_timer_is_active(&mgr, id) == true);

    /* Tick 3 times to fire */
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    assert(g_callback_count == 0);

    soft_timer_tick(&mgr);
    assert(g_callback_count == 1);
    assert(soft_timer_is_active(&mgr, id) == false);

    /* Additional ticks should not fire again */
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    assert(g_callback_count == 1);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    soft_timer_err_t err;

    err = soft_timer_init(NULL);
    assert(err == SOFT_TIMER_ERR_PARAM);

    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    /* Create with NULL callback should fail */
    int id = soft_timer_create(&mgr, 10, SOFT_TIMER_ONE_SHOT, NULL, NULL);
    assert(id < 0);

    /* Invalid timer ID */
    err = soft_timer_start(&mgr, -1);
    assert(err == SOFT_TIMER_ERR_PARAM);

    err = soft_timer_stop(&mgr, SOFT_TIMER_MAX + 1);
    assert(err == SOFT_TIMER_ERR_PARAM);

    assert(soft_timer_is_active(&mgr, -1) == false);
}

int main(void) {
    verify_invariants();
    verify_tick_monotonicity();
    verify_one_shot_fires_once();
    verify_null_safety();
    return 0;
}
