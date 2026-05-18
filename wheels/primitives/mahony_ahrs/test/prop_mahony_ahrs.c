// SPDX-License-Identifier: MIT
/*
 * prop_mahony_ahrs.c — property-based tests for mahony_ahrs
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_mahony_ahrs.c ../mahony_ahrs.c -o prop_mahony_ahrs -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mahony_ahrs.h"
#include <math.h>

/* Property: quaternion norm ~1.0 after many updates */
PROPERTY(quat_norm_unity, 10000) {
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_t f;
    mahony_ahrs_init(&f, &cfg);

    /* Feed random sensor data for a number of steps */
    unsigned steps = (unsigned)ARBI_RANGE(seed, 10, 100);
    for (unsigned i = 0; i < steps; i++) {
        float gx = (float)ARBI_RANGE(seed, -500, 500) / 10.0f;
        float gy = (float)ARBI_RANGE(seed, -500, 500) / 10.0f;
        float gz = (float)ARBI_RANGE(seed, -500, 500) / 10.0f;
        float ax = (float)ARBI_RANGE(seed, -1000, 1000) / 1000.0f;
        float ay = (float)ARBI_RANGE(seed, -1000, 1000) / 1000.0f;
        float az = (float)ARBI_RANGE(seed, -1000, 1000) / 1000.0f;
        mahony_ahrs_update(&f, gx, gy, gz, ax, ay, az, 0, 0, 0);
    }

    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);

    /* Norm should be close to 1.0 (allow small drift) */
    PROP_ASSERT(fabsf(norm - 1.0f) < 0.05f);
}

/* Property: initial state is identity quaternion [1,0,0,0] */
PROPERTY(initial_identity, 5000) {
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.sample_freq_hz = (float)ARBI_RANGE(seed, 50, 1000);
    mahony_ahrs_t f;
    mahony_ahrs_init(&f, &cfg);

    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);

    PROP_ASSERT(fabsf(q0 - 1.0f) < 0.001f);
    PROP_ASSERT(fabsf(q1) < 0.001f);
    PROP_ASSERT(fabsf(q2) < 0.001f);
    PROP_ASSERT(fabsf(q3) < 0.001f);
}

/* Property: stationary accel-only produces near-zero roll/pitch */
PROPERTY(stationary_level, 5000) {
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.kp = 10.0f;  /* High gain for fast convergence */
    mahony_ahrs_t f;
    mahony_ahrs_init(&f, &cfg);

    /* Feed gravity-only accel (0,0,1) with zero gyro for many steps */
    for (int i = 0; i < 500; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);

    /* Roll and pitch should be near zero */
    PROP_ASSERT(fabsf(e.roll_deg) < 5.0f);
    PROP_ASSERT(fabsf(e.pitch_deg) < 5.0f);
}

/* Property: euler angles are bounded */
PROPERTY(euler_bounded, 10000) {
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_t f;
    mahony_ahrs_init(&f, &cfg);

    unsigned steps = (unsigned)ARBI_RANGE(seed, 1, 50);
    for (unsigned i = 0; i < steps; i++) {
        float gx = (float)ARBI_RANGE(seed, -200, 200);
        float gy = (float)ARBI_RANGE(seed, -200, 200);
        float gz = (float)ARBI_RANGE(seed, -200, 200);
        mahony_ahrs_update(&f, gx, gy, gz, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);

    PROP_ASSERT(e.roll_deg >= -180.0f && e.roll_deg <= 180.0f);
    PROP_ASSERT(e.pitch_deg >= -90.0f && e.pitch_deg <= 90.0f);
    PROP_ASSERT(e.yaw_deg >= -180.0f && e.yaw_deg <= 360.0f);
}

TEST_MAIN("mahony_ahrs properties") {
    RUN(quat_norm_unity);
    RUN(initial_identity);
    RUN(stationary_level);
    RUN(euler_bounded);
}
TEST_MAIN_END()
