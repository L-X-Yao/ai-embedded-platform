/*
 * prop_debounce.c — property-based tests for debounce
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_debounce.c ../debounce.c -o prop_debounce
 */

#include "test_harness.h"
#include "property_test.h"
#include "../debounce.h"

/* Property: stable signal (N consecutive identical readings) passes through */
PROPERTY(stable_passes_through, 10000) {
    uint8_t threshold = (uint8_t)ARBI_RANGE(seed, 2, 20);
    bool target = (bool)(ARBI_UINT8(seed) & 1);
    debounce_t d;
    debounce_init(&d, threshold, !target);

    /* Feed target state for threshold readings */
    bool result = !target;
    for (uint8_t i = 0; i < threshold; i++) {
        result = debounce_update(&d, target);
    }

    PROP_ASSERT(result == target);
    PROP_ASSERT(debounce_get(&d) == target);
}

/* Property: bouncing signal (alternating) is filtered out */
PROPERTY(bouncing_filtered, 10000) {
    uint8_t threshold = (uint8_t)ARBI_RANGE(seed, 3, 20);
    debounce_t d;
    debounce_init(&d, threshold, false);

    bool initial_stable = debounce_get(&d);

    /* Feed alternating signal — never stable for threshold consecutive */
    unsigned bounces = (unsigned)ARBI_RANGE(seed, 10, 100);
    for (unsigned i = 0; i < bounces; i++) {
        bool raw = (bool)(i & 1);
        debounce_update(&d, raw);
    }

    /* Stable state should not have changed from initial */
    PROP_ASSERT(debounce_get(&d) == initial_stable);
}

/* Property: changed flag only true when stable state actually changed */
PROPERTY(changed_flag_correct, 5000) {
    uint8_t threshold = (uint8_t)ARBI_RANGE(seed, 2, 10);
    debounce_t d;
    debounce_init(&d, threshold, false);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 100);
    for (unsigned i = 0; i < ops; i++) {
        bool prev_stable = debounce_get(&d);
        bool raw = (bool)(ARBI_UINT8(seed) & 1);
        debounce_update(&d, raw);
        bool new_stable = debounce_get(&d);

        if (debounce_changed(&d)) {
            /* If changed flag is set, stable state must differ from previous */
            PROP_ASSERT(new_stable != prev_stable);
        } else {
            /* If not changed, stable state must be same */
            PROP_ASSERT(new_stable == prev_stable);
        }
    }
}

/* Property: rising edge only on false->true transition */
PROPERTY(rising_edge_correct, 5000) {
    uint8_t threshold = (uint8_t)ARBI_RANGE(seed, 2, 10);
    debounce_t d;
    debounce_init(&d, threshold, false);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 100);
    for (unsigned i = 0; i < ops; i++) {
        bool prev_stable = debounce_get(&d);
        bool raw = (bool)(ARBI_UINT8(seed) & 1);
        debounce_update(&d, raw);

        if (debounce_rising(&d)) {
            /* Rising: must have transitioned from false to true */
            PROP_ASSERT(prev_stable == false);
            PROP_ASSERT(debounce_get(&d) == true);
        }
    }
}

/* Property: output is always boolean (0 or 1) */
PROPERTY(output_boolean, 50000) {
    debounce_t d;
    debounce_init(&d, 5, false);

    bool raw = (bool)(ARBI_UINT8(seed) & 1);
    bool result = debounce_update(&d, raw);
    PROP_ASSERT(result == false || result == true);
    PROP_ASSERT(debounce_get(&d) == false || debounce_get(&d) == true);
}

TEST_MAIN("debounce properties") {
    RUN(stable_passes_through);
    RUN(bouncing_filtered);
    RUN(changed_flag_correct);
    RUN(rising_edge_correct);
    RUN(output_boolean);
}
TEST_MAIN_END()
