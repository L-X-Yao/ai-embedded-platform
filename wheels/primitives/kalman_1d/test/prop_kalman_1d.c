// SPDX-License-Identifier: MIT
/*
 * prop_kalman_1d.c -- property-based tests for kalman_1d
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_kalman_1d.c ../kalman_1d.c -o prop_kalman_1d -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../kalman_1d.h"
#include <math.h>

/* helper: generate a positive float in (0.001, 100] */
static float arbi_positive(uint32_t *seed)
{
    uint32_t raw = prop__xorshift32(seed) % 100000 + 1;
    return (float)raw / 1000.0f;  /* 0.001 to 100.0 */
}

/* helper: generate a float in [-1000, 1000] */
static float arbi_measurement(uint32_t *seed)
{
    int32_t raw = (int32_t)prop__xorshift32(seed);
    return (float)(raw % 1000000) / 1000.0f;
}

/* --- Property 1: constant measurements converge to that value --- */

PROPERTY(constant_converges, 5000) {
    float q = arbi_positive(&seed);
    float r = arbi_positive(&seed);
    float target = arbi_measurement(&seed);

    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = { q, r };
    kalman_1d_init(&kf, &cfg);

    float out = 0.0f;
    int i;
    for (i = 0; i < 200; i++) {
        out = kalman_1d_update(&kf, target);
    }

    float err = out - target;
    if (err < 0.0f) err = -err;
    float tol = (target < 0 ? -target : target) * 0.01f + 0.01f;
    PROP_ASSERT(err <= tol);
}

/* --- Property 2: Kalman gain stays in [0, 1] --- */

PROPERTY(gain_in_unit_range, 10000) {
    float q = arbi_positive(&seed);
    float r = arbi_positive(&seed);

    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = { q, r };
    kalman_1d_init(&kf, &cfg);

    int n = (int)(ARBI_SIZE(seed, 49)) + 2;
    int i;
    for (i = 0; i < n; i++) {
        float z = arbi_measurement(&seed);
        kalman_1d_update(&kf, z);
        float k = kalman_1d_get_gain(&kf);
        PROP_ASSERT(!(k < 0.0f));
        PROP_ASSERT(!(k > 1.0f + 1e-6f));
    }
}

/* --- Property 3: covariance P stays positive --- */

PROPERTY(covariance_stays_positive, 10000) {
    float q = arbi_positive(&seed);
    float r = arbi_positive(&seed);

    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = { q, r };
    kalman_1d_init(&kf, &cfg);

    int n = (int)(ARBI_SIZE(seed, 49)) + 2;
    int i;
    for (i = 0; i < n; i++) {
        float z = arbi_measurement(&seed);
        kalman_1d_update(&kf, z);
        PROP_ASSERT(!(kf.p < 0.0f));
    }
}

/* --- Property 4: NaN measurement does not corrupt state --- */

PROPERTY(nan_measurement_safe, 5000) {
    float q = arbi_positive(&seed);
    float r = arbi_positive(&seed);

    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = { q, r };
    kalman_1d_init(&kf, &cfg);

    /* feed a few valid measurements */
    int n = (int)(ARBI_SIZE(seed, 9)) + 1;
    int i;
    for (i = 0; i < n; i++) {
        kalman_1d_update(&kf, arbi_measurement(&seed));
    }

    float before = kalman_1d_get(&kf);
    float result = kalman_1d_update(&kf, (float)NAN);

    /* result should equal the pre-NaN estimate */
    float err = result - before;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);

    /* state x must not be NaN */
    float after = kalman_1d_get(&kf);
    PROP_ASSERT(after == after);  /* NaN != NaN by IEEE 754 */
}

/* --- Property 5: bootstrap sets state exactly to first measurement --- */

PROPERTY(bootstrap_exact, 10000) {
    float q = arbi_positive(&seed);
    float r = arbi_positive(&seed);
    float z = arbi_measurement(&seed);

    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = { q, r };
    kalman_1d_init(&kf, &cfg);

    float out = kalman_1d_update(&kf, z);

    float err = out - z;
    if (err < 0.0f) err = -err;
    PROP_ASSERT(err < 1e-6f);
}

TEST_MAIN("kalman_1d properties") {
    RUN(constant_converges);
    RUN(gain_in_unit_range);
    RUN(covariance_stays_positive);
    RUN(nan_measurement_safe);
    RUN(bootstrap_exact);
}
TEST_MAIN_END()
