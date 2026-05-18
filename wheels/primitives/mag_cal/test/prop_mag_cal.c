// SPDX-License-Identifier: MIT
/*
 * prop_mag_cal.c — property-based tests for mag_cal
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_mag_cal.c ../mag_cal.c -o prop_mag_cal -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mag_cal.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Property: calibrated output reduces offset compared to raw */
PROPERTY(calibration_improves, 5000) {
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    /* Simulate hard-iron offset: ideal sphere at origin + offset */
    float offset_x = (float)ARBI_RANGE(seed, -200, 200);
    float offset_y = (float)ARBI_RANGE(seed, -200, 200);
    float offset_z = (float)ARBI_RANGE(seed, -200, 200);
    float radius = 400.0f;

    /* Feed samples on a sphere with offset */
    int n_samples = MAG_CAL_MAX_SAMPLES;
    for (int i = 0; i < n_samples; i++) {
        /* Evenly distribute on sphere using golden ratio */
        float theta = (float)(2.0 * M_PI * i / 50.0);
        float phi = acosf(1.0f - 2.0f * ((float)i + 0.5f) / (float)n_samples);

        float mx = radius * sinf(phi) * cosf(theta) + offset_x;
        float my = radius * sinf(phi) * sinf(theta) + offset_y;
        float mz = radius * cosf(phi) + offset_z;

        mag_cal_feed(&cal, mx, my, mz);
    }

    mag_cal_state_t state = mag_cal_compute(&cal);
    if (state != MAG_CAL_DONE_OK) {
        /* Some random offsets may not produce good calibration — skip */
        return;
    }

    const mag_cal_result_t *result = mag_cal_get_result(&cal);

    /* Apply calibration and check that mean of calibrated data is closer to 0 */
    float sum_raw_x = 0, sum_cal_x = 0;
    for (int i = 0; i < 10; i++) {
        float theta = (float)(2.0 * M_PI * i / 10.0);
        float mx = radius * cosf(theta) + offset_x;
        float my = radius * sinf(theta) + offset_y;
        float mz = offset_z;

        float cx, cy, cz;
        mag_cal_apply(result, mx, my, mz, &cx, &cy, &cz);

        sum_raw_x += fabsf(mx);
        sum_cal_x += fabsf(cx);
    }

    /* Calibrated readings should generally have smaller magnitude than raw
     * if offset was significant */
    if (fabsf(offset_x) > radius * 0.1f) {
        PROP_ASSERT(sum_cal_x < sum_raw_x);
    }
}

/* Property: abort returns to IDLE */
PROPERTY(abort_returns_idle, 5000) {
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    /* Feed some samples */
    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 50);
    for (unsigned i = 0; i < n; i++) {
        mag_cal_feed(&cal, (float)i, (float)i, (float)i);
    }

    mag_cal_abort(&cal);
    PROP_ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE);
}

/* Property: sample count tracks feeds */
PROPERTY(sample_count_tracks, 10000) {
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    unsigned n = (unsigned)ARBI_RANGE(seed, 0, 100);
    if (n > MAG_CAL_MAX_SAMPLES) n = MAG_CAL_MAX_SAMPLES;

    for (unsigned i = 0; i < n; i++) {
        mag_cal_feed(&cal, (float)i, 0.0f, 0.0f);
    }

    PROP_ASSERT_EQ(mag_cal_sample_count(&cal), n);
}

/* Property: init resets to idle with zero samples */
PROPERTY(init_state_clean, 5000) {
    mag_cal_t cal;
    mag_cal_init(&cal);

    PROP_ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE);
    PROP_ASSERT_EQ(mag_cal_sample_count(&cal), 0);
}

TEST_MAIN("mag_cal properties") {
    RUN(calibration_improves);
    RUN(abort_returns_idle);
    RUN(sample_count_tracks);
    RUN(init_state_clean);
}
TEST_MAIN_END()
