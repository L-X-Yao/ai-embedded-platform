// SPDX-License-Identifier: MIT
/*
 * prop_state_machine.c — property-based tests for state_machine
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_state_machine.c ../state_machine.c -o prop_state_machine
 */

#include "test_harness.h"
#include "property_test.h"
#include "../state_machine.h"

/* Define a small FSM for testing: 3 states, 3 events */
enum { ST_A = 0, ST_B, ST_C, ST_COUNT };
enum { EV_X = 0, EV_Y, EV_Z, EV_COUNT };

static const sm_transition_t test_transitions[] = {
    { ST_A, EV_X, NULL, ST_B, NULL },
    { ST_A, EV_Y, NULL, ST_C, NULL },
    { ST_B, EV_Y, NULL, ST_C, NULL },
    { ST_B, EV_Z, NULL, ST_A, NULL },
    { ST_C, EV_X, NULL, ST_A, NULL },
    { ST_C, EV_Z, NULL, ST_B, NULL },
};

static const sm_state_def_t test_state_defs[] = {
    { ST_A, NULL, NULL },
    { ST_B, NULL, NULL },
    { ST_C, NULL, NULL },
};

/* Property: current state always valid after random event sequences */
PROPERTY(state_always_valid, 10000) {
    state_machine_t sm;
    sm_cfg_t cfg = {
        .transitions     = test_transitions,
        .num_transitions = sizeof(test_transitions) / sizeof(test_transitions[0]),
        .state_defs      = test_state_defs,
        .num_state_defs  = ST_COUNT,
        .user_ctx        = NULL,
    };
    state_machine_init(&sm, &cfg, ST_A);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 100);
    for (unsigned i = 0; i < ops; i++) {
        sm_event_t ev = (sm_event_t)(ARBI_UINT8(seed) % (EV_COUNT + 2));
        /* Feed events including invalid ones beyond EV_COUNT */
        state_machine_process(&sm, ev);

        sm_state_t cur = state_machine_current(&sm);
        PROP_ASSERT(cur < ST_COUNT);
    }
}

/* Property: reset returns to specified initial state */
PROPERTY(reset_returns_to_initial, 10000) {
    state_machine_t sm;
    sm_cfg_t cfg = {
        .transitions     = test_transitions,
        .num_transitions = sizeof(test_transitions) / sizeof(test_transitions[0]),
        .state_defs      = test_state_defs,
        .num_state_defs  = ST_COUNT,
        .user_ctx        = NULL,
    };
    sm_state_t init_state = (sm_state_t)(ARBI_UINT8(seed) % ST_COUNT);
    state_machine_init(&sm, &cfg, init_state);

    /* Drive FSM through random events */
    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 50);
    for (unsigned i = 0; i < ops; i++) {
        sm_event_t ev = (sm_event_t)(ARBI_UINT8(seed) % EV_COUNT);
        state_machine_process(&sm, ev);
    }

    /* Reset to a specific state */
    sm_state_t reset_target = (sm_state_t)(ARBI_UINT8(seed) % ST_COUNT);
    state_machine_err_t err = state_machine_reset(&sm, reset_target);
    PROP_ASSERT_EQ(err, STATE_MACHINE_OK);
    PROP_ASSERT_EQ(state_machine_current(&sm), reset_target);
}

/* Property: transition count is monotonically non-decreasing */
PROPERTY(transition_count_monotonic, 5000) {
    state_machine_t sm;
    sm_cfg_t cfg = {
        .transitions     = test_transitions,
        .num_transitions = sizeof(test_transitions) / sizeof(test_transitions[0]),
        .state_defs      = test_state_defs,
        .num_state_defs  = ST_COUNT,
        .user_ctx        = NULL,
    };
    state_machine_init(&sm, &cfg, ST_A);

    uint32_t prev_count = state_machine_transition_count(&sm);
    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 100);
    for (unsigned i = 0; i < ops; i++) {
        sm_event_t ev = (sm_event_t)(ARBI_UINT8(seed) % EV_COUNT);
        state_machine_process(&sm, ev);
        uint32_t cur_count = state_machine_transition_count(&sm);
        PROP_ASSERT(cur_count >= prev_count);
        prev_count = cur_count;
    }
}

/* Property: no-crash on invalid events (beyond defined range) */
PROPERTY(no_crash_invalid_events, 50000) {
    state_machine_t sm;
    sm_cfg_t cfg = {
        .transitions     = test_transitions,
        .num_transitions = sizeof(test_transitions) / sizeof(test_transitions[0]),
        .state_defs      = test_state_defs,
        .num_state_defs  = ST_COUNT,
        .user_ctx        = NULL,
    };
    state_machine_init(&sm, &cfg, ST_A);

    sm_event_t ev = (sm_event_t)ARBI_UINT16(seed);
    state_machine_err_t err = state_machine_process(&sm, ev);
    /* Either OK or ERR_NO_TRANSITION, but never crash */
    PROP_ASSERT(err == STATE_MACHINE_OK || err == STATE_MACHINE_ERR_NO_TRANSITION);
    PROP_ASSERT(state_machine_current(&sm) < ST_COUNT);
}

TEST_MAIN("state_machine properties") {
    RUN(state_always_valid);
    RUN(reset_returns_to_initial);
    RUN(transition_count_monotonic);
    RUN(no_crash_invalid_events);
}
TEST_MAIN_END()
