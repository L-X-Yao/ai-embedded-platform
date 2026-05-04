/*
 * verify_debounce.c — CBMC bounded model checking for debounce
 *
 * Verifies stable state transitions, threshold behavior, and edge detection.
 * Run with:
 *   cbmc verify_debounce.c ../debounce.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../debounce.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern unsigned  nondet_unsigned(void);

/* Proof 1: N consecutive identical readings cause state transition */
void verify_threshold_transition(void) {
    debounce_t d;
    uint8_t threshold = nondet_uint8();
    __CPROVER_assume(threshold >= 2 && threshold <= 10);

    debounce_init(&d, threshold, false);
    assert(debounce_get(&d) == false);

    /* Feed (threshold - 1) true readings — should NOT transition */
    for (uint8_t i = 0; i < threshold - 1; i++) {
        debounce_update(&d, true);
    }
    assert(debounce_get(&d) == false);

    /* One more true reading — should transition */
    debounce_update(&d, true);
    assert(debounce_get(&d) == true);
    assert(debounce_changed(&d) == true);
    assert(debounce_rising(&d) == true);
    assert(debounce_falling(&d) == false);
}

/* Proof 2: intermittent noise does not cause false transition */
void verify_noise_rejection(void) {
    debounce_t d;
    uint8_t threshold = 5;
    debounce_init(&d, threshold, false);

    /* Feed alternating readings — never reaches threshold */
    for (int i = 0; i < 20; i++) {
        bool sample = (i % 2 == 0);
        debounce_update(&d, sample);
        /* Alternating resets the counter, stable stays false */
        assert(debounce_get(&d) == false);
    }
}

/* Proof 3: rising and falling edge detection */
void verify_edge_detection(void) {
    debounce_t d;
    uint8_t threshold = 3;
    debounce_init(&d, threshold, false);

    /* Transition to true: rising edge */
    for (uint8_t i = 0; i < threshold; i++) {
        debounce_update(&d, true);
    }
    assert(debounce_rising(&d) == true);
    assert(debounce_falling(&d) == false);
    assert(debounce_changed(&d) == true);

    /* Sustain true — no edge */
    debounce_update(&d, true);
    assert(debounce_changed(&d) == false);
    assert(debounce_rising(&d) == false);
    assert(debounce_falling(&d) == false);

    /* Transition back to false: falling edge */
    for (uint8_t i = 0; i < threshold; i++) {
        debounce_update(&d, false);
    }
    assert(debounce_falling(&d) == true);
    assert(debounce_rising(&d) == false);
    assert(debounce_changed(&d) == true);

    /* Sustain false — no edge */
    debounce_update(&d, false);
    assert(debounce_changed(&d) == false);
}

/* Proof 4: invariants hold after any sequence of updates */
void verify_invariants(void) {
    debounce_t d;
    uint8_t threshold = nondet_uint8();
    __CPROVER_assume(threshold >= 1 && threshold <= 20);

    bool init_state = (nondet_uint8() & 1) != 0;
    debounce_init(&d, threshold, init_state);

    assert(debounce_get(&d) == init_state);
    assert(d.threshold == threshold);
    assert(d.counter == 0);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    bool prev_stable = init_state;
    for (unsigned i = 0; i < ops; i++) {
        bool sample = (nondet_uint8() & 1) != 0;
        bool result = debounce_update(&d, sample);

        /* Return value matches get */
        assert(result == debounce_get(&d));

        /* Counter never exceeds threshold */
        assert(d.counter <= d.threshold);

        /* Changed flag is consistent */
        bool curr_stable = debounce_get(&d);
        if (debounce_changed(&d)) {
            assert(curr_stable != prev_stable);
        }

        /* Rising/falling mutually exclusive */
        assert(!(debounce_rising(&d) && debounce_falling(&d)));

        prev_stable = curr_stable;
    }
}

int main(void) {
    verify_threshold_transition();
    verify_noise_rejection();
    verify_edge_detection();
    verify_invariants();
    return 0;
}
