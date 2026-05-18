// SPDX-License-Identifier: MIT
/*
 * prop_iir_lowpass.c -- property-based tests for iir_lowpass
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_iir_lowpass.c ../iir_lowpass.c -o prop_iir_lowpass -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../iir_lowpass.h"
#include <math.h>

/* helper: generate alpha in (0, 1] from seed */
static float arbi_alpha(uint32_t *seed)
{
    /* Map to (0, 1]: use 1..1000 then divide by 1000 */
    uint32_t raw = prop__xorshift32(seed) % 1000 + 1;
    return (float)raw / 1000.0f;
}

/* helper: generate a finite float in [-1e6, 1e6] */
static float arbi_float(uint32_t *seed)
{
    int32_t raw = (int32_t)prop__xorshift32(seed);
    return (float)raw / 2147.0f;  /* range roughly +/- 1e6 */
}

/* --- Property 1: constant input converges to that constant --- */

PROPERTY(constant_input_converges, 5000) {
    float alpha = arbi_alpha(&seed);
    float target = arbi_float(&seed);

    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = { alpha };
    iir_lowpass_init(&f, &cfg);

    int i;
    float out = 0.0f;
    for (i = 0; i < 200; i++) {
        out = iir_lowpass_feed(&f, target);
    }

    float err = out - target;
    if (err < 0.0f) err = -err;
    /* Must converge within 1% of target magnitude or 0.01 absolute */
    float tol = (target < 0 ? -target : target) * 0.01f + 0.01f;
    PROP_ASSERT(err <= tol);
}

/* --- Property 2: output bounded by input range --- */

PROPERTY(output_bounded_by_input_range, 10000) {
    float alpha = arbi_alpha(&seed);

    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = { alpha };
    iir_lowpass_init(&f, &cfg);

    float lo = 1e30f, hi = -1e30f;
    int i;
    int n = (int)ARBI_SIZE(seed, 49) + 2;
    for (i = 0; i < n; i++) {
        float x = arbi_float(&seed);
        if (x < lo) lo = x;
        if (x > hi) hi = x;
        float out = iir_lowpass_feed(&f, x);
        /* After first sample, output is always within [lo, hi] */
        PROP_ASSERT(!(out < lo - 1e-3f));
        PROP_ASSERT(!(out > hi + 1e-3f));
    }
}

/* --- Property 3: alpha=1 is pass-through --- */

PROPERTY(alpha_one_passthrough, 10000) {
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = { 1.0f };
    iir_lowpass_init(&f, &cfg);

    float x = arbi_float(&seed);
    float out = iir_lowpass_feed(&f, x);

    /* first sample is always passthrough regardless of alpha */
    float err = out - x;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);

    /* subsequent samples should also be passthrough with alpha=1 */
    x = arbi_float(&seed);
    out = iir_lowpass_feed(&f, x);
    err = out - x;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);
}

/* --- Property 4: first sample is always pass-through --- */

PROPERTY(first_sample_passthrough, 10000) {
    float alpha = arbi_alpha(&seed);
    float x = arbi_float(&seed);

    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = { alpha };
    iir_lowpass_init(&f, &cfg);

    float out = iir_lowpass_feed(&f, x);
    float err = out - x;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);
}

/* --- Property 5: reset restores to initial state --- */

PROPERTY(reset_restores_initial, 5000) {
    float alpha = arbi_alpha(&seed);

    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = { alpha };
    iir_lowpass_init(&f, &cfg);

    /* feed some random data */
    int n = (int)ARBI_SIZE(seed, 19) + 1;
    int i;
    for (i = 0; i < n; i++) {
        iir_lowpass_feed(&f, arbi_float(&seed));
    }

    iir_lowpass_reset(&f);

    /* after reset, first feed should be passthrough again */
    float x = arbi_float(&seed);
    float out = iir_lowpass_feed(&f, x);
    float err = out - x;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);
}

TEST_MAIN("iir_lowpass properties") {
    RUN(constant_input_converges);
    RUN(output_bounded_by_input_range);
    RUN(alpha_one_passthrough);
    RUN(first_sample_passthrough);
    RUN(reset_restores_initial);
}
TEST_MAIN_END()
