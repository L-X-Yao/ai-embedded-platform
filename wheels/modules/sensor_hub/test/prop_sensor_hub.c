// SPDX-License-Identifier: MIT
/*
 * prop_sensor_hub.c — property-based tests for sensor_hub module
 *
 * Properties:
 *   1. register+poll never crashes with arbitrary sensor values
 *   2. stats (min/max/avg) are consistent after polls
 *   3. register beyond max returns ERR_FULL
 *   4. get with out-of-range index returns error
 */

#include "test_harness.h"
#include "property_test.h"
#include "sensor_hub.h"
#include <string.h>

/* ---------- mock sensor callbacks ---------- */

static float mock_sensor_values[SENSOR_HUB_MAX_SENSORS];
static sensor_hub_err_t mock_sensor_errs[SENSOR_HUB_MAX_SENSORS];

static sensor_hub_err_t mock_read_0(void *ctx, float *value)
{
    (void)ctx;
    if (mock_sensor_errs[0] != SENSOR_HUB_OK) return mock_sensor_errs[0];
    *value = mock_sensor_values[0];
    return SENSOR_HUB_OK;
}

static sensor_hub_err_t mock_read_1(void *ctx, float *value)
{
    (void)ctx;
    if (mock_sensor_errs[1] != SENSOR_HUB_OK) return mock_sensor_errs[1];
    *value = mock_sensor_values[1];
    return SENSOR_HUB_OK;
}

static sensor_hub_err_t mock_read_2(void *ctx, float *value)
{
    (void)ctx;
    if (mock_sensor_errs[2] != SENSOR_HUB_OK) return mock_sensor_errs[2];
    *value = mock_sensor_values[2];
    return SENSOR_HUB_OK;
}

static const sensor_hub_read_fn mock_reads[] = {
    mock_read_0, mock_read_1, mock_read_2,
};

static void mock_reset(void)
{
    for (int i = 0; i < SENSOR_HUB_MAX_SENSORS; i++) {
        mock_sensor_values[i] = 0.0f;
        mock_sensor_errs[i] = SENSOR_HUB_OK;
    }
}

/* ------------------------------------------------------------------ */

PROPERTY(register_poll_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    sensor_hub_t hub;
    sensor_hub_init(&hub);

    int n = (int)(1 + ARBI_UINT8(seed) % 3);
    for (int i = 0; i < n; i++) {
        mock_sensor_values[i] = (float)((int32_t)ARBI_UINT32(seed)) / 100.0f;
        sensor_hub_register(&hub, "sensor", "unit", mock_reads[i], NULL);
    }

    /* Poll multiple times with changing values */
    int polls = (int)(1 + ARBI_UINT8(seed) % 10);
    for (int p = 0; p < polls; p++) {
        for (int i = 0; i < n; i++)
            mock_sensor_values[i] = (float)((int32_t)ARBI_UINT32(seed)) / 100.0f;
        sensor_hub_poll(&hub);
    }

    /* Read back values — must not crash */
    for (int i = 0; i < n; i++) {
        float val;
        sensor_hub_status_t st;
        sensor_hub_get(&hub, (uint8_t)i, &val, &st);
    }
}

PROPERTY(stats_consistent, 5000)
{
    mock_reset();
    sensor_hub_t hub;
    sensor_hub_init(&hub);
    sensor_hub_register(&hub, "temp", "C", mock_read_0, NULL);

    /* Poll with multiple values, track expected min/max */
    float exp_min = 1e30f, exp_max = -1e30f;
    int polls = (int)(2 + ARBI_UINT8(seed) % 20);
    for (int p = 0; p < polls; p++) {
        float v = (float)((int32_t)ARBI_UINT32(seed)) / 1000.0f;
        mock_sensor_values[0] = v;
        sensor_hub_poll(&hub);
        if (v < exp_min) exp_min = v;
        if (v > exp_max) exp_max = v;
    }

    float s_min, s_max, s_avg;
    sensor_hub_err_t err = sensor_hub_get_stats(&hub, 0, &s_min, &s_max, &s_avg);
    if (err != SENSOR_HUB_OK) return;

    PROP_ASSERT(s_min == s_min); /* not NaN */
    PROP_ASSERT(s_max == s_max);
    PROP_ASSERT(s_avg == s_avg);
    PROP_ASSERT(s_min <= s_max);
    /* Verify min/max match our tracking (float exact since same values) */
    PROP_ASSERT(s_min == exp_min);
    PROP_ASSERT(s_max == exp_max);
}

PROPERTY(register_full_returns_error, 5000)
{
    mock_reset();
    sensor_hub_t hub;
    sensor_hub_init(&hub);

    /* Fill all slots */
    for (int i = 0; i < SENSOR_HUB_MAX_SENSORS; i++)
        sensor_hub_register(&hub, "s", "u", mock_read_0, NULL);

    /* One more should return ERR_FULL */
    sensor_hub_err_t err = sensor_hub_register(&hub, "extra", "u",
                                                mock_read_0, NULL);
    PROP_ASSERT(err == SENSOR_HUB_ERR_FULL);

    /* Poll should still work */
    sensor_hub_poll(&hub);
}

PROPERTY(get_oob_returns_error, 5000)
{
    mock_reset();
    sensor_hub_t hub;
    sensor_hub_init(&hub);
    sensor_hub_register(&hub, "s", "u", mock_read_0, NULL);

    uint8_t idx = (uint8_t)(1 + ARBI_UINT8(seed));
    if (idx == 0) idx = 2; /* ensure out of range */
    float val;
    sensor_hub_status_t st;
    sensor_hub_err_t err = sensor_hub_get(&hub, idx, &val, &st);
    PROP_ASSERT(err != SENSOR_HUB_OK);
}

TEST_MAIN("prop_sensor_hub")
    RUN(register_poll_no_crash);
    RUN(stats_consistent);
    RUN(register_full_returns_error);
    RUN(get_oob_returns_error);
TEST_MAIN_END()
