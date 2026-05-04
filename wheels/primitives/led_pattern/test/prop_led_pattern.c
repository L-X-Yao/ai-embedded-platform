/*
 * prop_led_pattern.c — property-based tests for led_pattern
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_led_pattern.c ../led_pattern.c -o prop_led_pattern
 */

#include "test_harness.h"
#include "property_test.h"
#include "../led_pattern.h"

/* Property: tick never crashes on any pattern configuration */
PROPERTY(tick_no_crash, 10000) {
    led_step_t steps[4];
    unsigned nsteps = (unsigned)ARBI_RANGE(seed, 1, 4);
    for (unsigned i = 0; i < nsteps; i++) {
        steps[i].on_ticks  = (uint16_t)ARBI_RANGE(seed, 1, 100);
        steps[i].off_ticks = (uint16_t)ARBI_RANGE(seed, 1, 100);
    }

    led_pattern_cfg_t cfg = {
        .steps = steps,
        .step_count = (uint8_t)nsteps,
        .loop = (ARBI_UINT8(seed) % 2) ? true : false
    };

    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    unsigned ticks = (unsigned)ARBI_RANGE(seed, 1, 500);
    for (unsigned i = 0; i < ticks; i++) {
        led_pattern_tick(&p);
    }
    PROP_ASSERT(1);
}

/* Property: looping pattern never finishes */
PROPERTY(loop_never_finishes, 5000) {
    led_step_t steps[2] = {
        { .on_ticks = (uint16_t)ARBI_RANGE(seed, 1, 10),
          .off_ticks = (uint16_t)ARBI_RANGE(seed, 1, 10) },
        { .on_ticks = (uint16_t)ARBI_RANGE(seed, 1, 10),
          .off_ticks = (uint16_t)ARBI_RANGE(seed, 1, 10) }
    };

    led_pattern_cfg_t cfg = {
        .steps = steps,
        .step_count = 2,
        .loop = true
    };

    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    /* Tick many times — should never report finished */
    for (int i = 0; i < 1000; i++) {
        led_pattern_tick(&p);
        PROP_ASSERT(!led_pattern_finished(&p));
    }
}

/* Property: one-shot pattern eventually finishes */
PROPERTY(oneshot_finishes, 5000) {
    led_step_t steps[2] = {
        { .on_ticks = (uint16_t)ARBI_RANGE(seed, 1, 5),
          .off_ticks = (uint16_t)ARBI_RANGE(seed, 1, 5) },
        { .on_ticks = (uint16_t)ARBI_RANGE(seed, 1, 5),
          .off_ticks = (uint16_t)ARBI_RANGE(seed, 1, 5) }
    };

    uint16_t max_ticks = 0;
    for (int i = 0; i < 2; i++)
        max_ticks += steps[i].on_ticks + steps[i].off_ticks;

    led_pattern_cfg_t cfg = {
        .steps = steps,
        .step_count = 2,
        .loop = false
    };

    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    /* Tick enough to complete all steps */
    for (uint16_t i = 0; i < max_ticks + 10; i++) {
        led_pattern_tick(&p);
    }
    PROP_ASSERT(led_pattern_finished(&p));
}

/* Property: restart resets finished state */
PROPERTY(restart_resets, 5000) {
    led_step_t steps[1] = {
        { .on_ticks = 2, .off_ticks = 2 }
    };

    led_pattern_cfg_t cfg = {
        .steps = steps,
        .step_count = 1,
        .loop = false
    };

    led_pattern_t p;
    led_pattern_init(&p, &cfg);

    /* Complete the pattern */
    for (int i = 0; i < 10; i++) led_pattern_tick(&p);
    PROP_ASSERT(led_pattern_finished(&p));

    /* Restart and verify not finished */
    led_pattern_restart(&p);
    PROP_ASSERT(!led_pattern_finished(&p));
}

TEST_MAIN("led_pattern properties") {
    RUN(tick_no_crash);
    RUN(loop_never_finishes);
    RUN(oneshot_finishes);
    RUN(restart_resets);
}
TEST_MAIN_END()
