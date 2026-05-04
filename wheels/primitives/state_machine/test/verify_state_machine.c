/*
 * verify_state_machine.c — CBMC bounded model checking for state_machine
 *
 * Proves: no invalid state reachable, transition count monotonic,
 * reset returns to specified state.
 *
 * Run: cbmc verify_state_machine.c ../state_machine.c --unwind 51 \
 *      --bounds-check --pointer-check
 */

#include "../state_machine.h"
#include <assert.h>

extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

#define NUM_STATES 4
#define NUM_EVENTS 3

enum { S0 = 0, S1, S2, S3 };
enum { E_A = 0, E_B, E_C };

static const sm_transition_t transitions[] = {
    { S0, E_A, NULL, S1, NULL },
    { S0, E_B, NULL, S2, NULL },
    { S1, E_A, NULL, S2, NULL },
    { S1, E_C, NULL, S0, NULL },
    { S2, E_B, NULL, S3, NULL },
    { S2, E_C, NULL, S0, NULL },
    { S3, E_A, NULL, S0, NULL },
    { S3, E_C, NULL, S1, NULL },
};

static const sm_cfg_t cfg = {
    .transitions = transitions,
    .num_transitions = sizeof(transitions) / sizeof(transitions[0]),
    .state_defs = NULL,
    .num_state_defs = 0,
    .user_ctx = NULL,
};

/* Proof 1: current state always valid after any event sequence */
void verify_state_always_valid(void) {
    state_machine_t sm;
    state_machine_init(&sm, &cfg, S0);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= 50);

    for (unsigned i = 0; i < ops; i++) {
        sm_event_t evt = (sm_event_t)(nondet_uint16() % NUM_EVENTS);
        state_machine_process(&sm, evt);

        sm_state_t cur = state_machine_current(&sm);
        assert(cur < NUM_STATES);
    }
}

/* Proof 2: transition count is monotonically non-decreasing */
void verify_transition_count_monotonic(void) {
    state_machine_t sm;
    state_machine_init(&sm, &cfg, S0);

    uint32_t prev_count = 0;
    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= 50);

    for (unsigned i = 0; i < ops; i++) {
        sm_event_t evt = (sm_event_t)(nondet_uint16() % NUM_EVENTS);
        state_machine_process(&sm, evt);

        uint32_t cur_count = state_machine_transition_count(&sm);
        assert(cur_count >= prev_count);
        prev_count = cur_count;
    }
}

/* Proof 3: reset returns to specified state */
void verify_reset(void) {
    state_machine_t sm;
    state_machine_init(&sm, &cfg, S0);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= 20);
    for (unsigned i = 0; i < ops; i++) {
        sm_event_t evt = (sm_event_t)(nondet_uint16() % NUM_EVENTS);
        state_machine_process(&sm, evt);
    }

    sm_state_t reset_to = (sm_state_t)(nondet_uint16() % NUM_STATES);
    state_machine_err_t err = state_machine_reset(&sm, reset_to);
    assert(err == STATE_MACHINE_OK);
    assert(state_machine_current(&sm) == reset_to);
}

/* Proof 4: NULL parameter safety */
void verify_null_safety(void) {
    state_machine_err_t err;
    err = state_machine_init(NULL, &cfg, S0);
    assert(err != STATE_MACHINE_OK);

    state_machine_t sm;
    err = state_machine_init(&sm, NULL, S0);
    assert(err != STATE_MACHINE_OK);

    err = state_machine_init(&sm, &cfg, S0);
    assert(err == STATE_MACHINE_OK);

    err = state_machine_process(NULL, E_A);
    assert(err != STATE_MACHINE_OK);
}

int main(void) {
    verify_state_always_valid();
    verify_transition_count_monotonic();
    verify_reset();
    verify_null_safety();
    return 0;
}
