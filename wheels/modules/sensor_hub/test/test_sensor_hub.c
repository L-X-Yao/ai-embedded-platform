// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "sensor_hub.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* ---- Mock sensor callbacks ---- */

static float mock_temp_value = 25.0f;
static sensor_hub_err_t mock_temp_err = SENSOR_HUB_OK;

static sensor_hub_err_t mock_temp_read(void *ctx, float *value)
{
    (void)ctx;
    if (mock_temp_err != SENSOR_HUB_OK) {
        return mock_temp_err;
    }
    *value = mock_temp_value;
    return SENSOR_HUB_OK;
}

static float mock_hum_value = 60.0f;
static sensor_hub_err_t mock_hum_err = SENSOR_HUB_OK;

static sensor_hub_err_t mock_hum_read(void *ctx, float *value)
{
    (void)ctx;
    if (mock_hum_err != SENSOR_HUB_OK) {
        return mock_hum_err;
    }
    *value = mock_hum_value;
    return SENSOR_HUB_OK;
}

static int ctx_read_called = 0;

static sensor_hub_err_t mock_ctx_read(void *ctx, float *value)
{
    int *flag = (int *)ctx;
    *flag = 42;
    ctx_read_called = 1;
    *value = 1.0f;
    return SENSOR_HUB_OK;
}

/* ---- Helpers ---- */

static sensor_hub_t hub;

static void setup(void)
{
    mock_temp_value = 25.0f;
    mock_temp_err   = SENSOR_HUB_OK;
    mock_hum_value  = 60.0f;
    mock_hum_err    = SENSOR_HUB_OK;
    ctx_read_called = 0;
    sensor_hub_init(&hub);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    sensor_hub_err_t err = sensor_hub_init(&hub);
    ASSERT_EQ(err, SENSOR_HUB_OK);
    ASSERT_EQ(hub.sensor_count, 0);
}

TEST(test_init_null)
{
    sensor_hub_err_t err = sensor_hub_init(NULL);
    ASSERT_EQ(err, SENSOR_HUB_ERR_PARAM);
}

TEST(test_register_and_poll)
{
    setup();
    sensor_hub_err_t err;
    err = sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    ASSERT_EQ(err, SENSOR_HUB_OK);
    ASSERT_EQ(hub.sensor_count, 1);

    err = sensor_hub_poll(&hub);
    ASSERT_EQ(err, SENSOR_HUB_OK);

    float val = 0.0f;
    sensor_hub_status_t st = SENSOR_HUB_STATUS_STALE;
    err = sensor_hub_get(&hub, 0, &val, &st);
    ASSERT_EQ(err, SENSOR_HUB_OK);
    ASSERT_NEAR(val, 25.0f, 0.001f);
    ASSERT_EQ(st, SENSOR_HUB_STATUS_OK);
}

TEST(test_register_max_sensors_full)
{
    setup();
    sensor_hub_err_t err;
    char names[SENSOR_HUB_MAX_SENSORS + 1][8];
    for (int i = 0; i < SENSOR_HUB_MAX_SENSORS; i++) {
        snprintf(names[i], sizeof(names[i]), "s%d", i);
        err = sensor_hub_register(&hub, names[i], "", mock_temp_read, NULL);
        ASSERT_EQ(err, SENSOR_HUB_OK);
    }
    /* next register should fail */
    err = sensor_hub_register(&hub, "overflow", "", mock_temp_read, NULL);
    ASSERT_EQ(err, SENSOR_HUB_ERR_FULL);
    ASSERT_EQ(hub.sensor_count, SENSOR_HUB_MAX_SENSORS);
}

TEST(test_poll_sensor_error)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    mock_temp_err = SENSOR_HUB_ERR_READ;

    sensor_hub_err_t err = sensor_hub_poll(&hub);
    ASSERT_EQ(err, SENSOR_HUB_ERR_READ);

    float val = 0.0f;
    sensor_hub_status_t st;
    sensor_hub_get(&hub, 0, &val, &st);
    ASSERT_EQ(st, SENSOR_HUB_STATUS_ERROR);
    ASSERT_EQ(hub.sensors[0].error_count, 1u);

    /* poll again with error -- error_count increments */
    sensor_hub_poll(&hub);
    ASSERT_EQ(hub.sensors[0].error_count, 2u);
}

TEST(test_stats_min_max_avg)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    /* Poll with different values */
    mock_temp_value = 10.0f;
    sensor_hub_poll(&hub);
    mock_temp_value = 30.0f;
    sensor_hub_poll(&hub);
    mock_temp_value = 20.0f;
    sensor_hub_poll(&hub);

    float min = 0.0f, max = 0.0f, avg = 0.0f;
    sensor_hub_err_t err = sensor_hub_get_stats(&hub, 0, &min, &max, &avg);
    ASSERT_EQ(err, SENSOR_HUB_OK);
    ASSERT_NEAR(min, 10.0f, 0.001f);
    ASSERT_NEAR(max, 30.0f, 0.001f);
    ASSERT_NEAR(avg, 20.0f, 0.001f);  /* (10+30+20)/3 = 20 */
}

TEST(test_reset_stats)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    mock_temp_value = 50.0f;
    sensor_hub_poll(&hub);

    sensor_hub_reset_stats(&hub);

    float min = -1.0f, max = -1.0f, avg = -1.0f;
    sensor_hub_get_stats(&hub, 0, &min, &max, &avg);
    ASSERT_NEAR(min, 0.0f, 0.001f);
    ASSERT_NEAR(max, 0.0f, 0.001f);
    ASSERT_NEAR(avg, 0.0f, 0.001f);
    ASSERT_EQ(hub.sensors[0].count, 0u);
    ASSERT_EQ(hub.sensors[0].error_count, 0u);
}

TEST(test_find_by_name)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    sensor_hub_register(&hub, "hum", "%", mock_hum_read, NULL);

    int idx = sensor_hub_find(&hub, "hum");
    ASSERT_EQ(idx, 1);

    idx = sensor_hub_find(&hub, "temp");
    ASSERT_EQ(idx, 0);

    idx = sensor_hub_find(&hub, "nonexistent");
    ASSERT_EQ(idx, -1);
}

TEST(test_find_null_params)
{
    setup();
    ASSERT_EQ(sensor_hub_find(NULL, "x"), -1);
    ASSERT_EQ(sensor_hub_find(&hub, NULL), -1);
}

TEST(test_multiple_sensors_polled)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    sensor_hub_register(&hub, "hum", "%", mock_hum_read, NULL);

    mock_temp_value = 22.5f;
    mock_hum_value  = 55.0f;
    sensor_hub_err_t err = sensor_hub_poll(&hub);
    ASSERT_EQ(err, SENSOR_HUB_OK);

    float val = 0.0f;
    sensor_hub_get(&hub, 0, &val, NULL);
    ASSERT_NEAR(val, 22.5f, 0.001f);

    sensor_hub_get(&hub, 1, &val, NULL);
    ASSERT_NEAR(val, 55.0f, 0.001f);
}

TEST(test_null_hub_params)
{
    ASSERT_EQ(sensor_hub_poll(NULL), SENSOR_HUB_ERR_PARAM);
    ASSERT_EQ(sensor_hub_get(NULL, 0, NULL, NULL), SENSOR_HUB_ERR_PARAM);
    ASSERT_EQ(sensor_hub_get_stats(NULL, 0, NULL, NULL, NULL),
              SENSOR_HUB_ERR_PARAM);
    ASSERT_EQ(sensor_hub_count(NULL), 0);

    /* reset_stats with NULL should not crash */
    sensor_hub_reset_stats(NULL);
    ASSERT_TRUE(1);
}

TEST(test_get_invalid_index)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    float val;
    sensor_hub_status_t st;
    sensor_hub_err_t err = sensor_hub_get(&hub, 5, &val, &st);
    ASSERT_EQ(err, SENSOR_HUB_ERR_PARAM);

    err = sensor_hub_get_stats(&hub, 5, NULL, NULL, NULL);
    ASSERT_EQ(err, SENSOR_HUB_ERR_PARAM);
}

TEST(test_varying_values_stats)
{
    setup();
    sensor_hub_register(&hub, "pressure", "hPa", mock_temp_read, NULL);

    float values[] = {1013.0f, 1010.0f, 1015.0f, 1012.0f, 1011.0f};
    float expected_sum = 0.0f;
    float expected_min = values[0];
    float expected_max = values[0];

    for (int i = 0; i < 5; i++) {
        mock_temp_value = values[i];
        sensor_hub_poll(&hub);
        expected_sum += values[i];
        if (values[i] < expected_min) { expected_min = values[i]; }
        if (values[i] > expected_max) { expected_max = values[i]; }
    }

    float min, max, avg;
    sensor_hub_get_stats(&hub, 0, &min, &max, &avg);
    ASSERT_NEAR(min, expected_min, 0.1f);
    ASSERT_NEAR(max, expected_max, 0.1f);
    ASSERT_NEAR(avg, expected_sum / 5.0f, 0.1f);
}

TEST(test_zero_readings_avg)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    /* no polls yet -- avg should be 0 */
    float avg = -1.0f;
    sensor_hub_get_stats(&hub, 0, NULL, NULL, &avg);
    ASSERT_NEAR(avg, 0.0f, 0.001f);
}

TEST(test_sensor_count)
{
    setup();
    ASSERT_EQ(sensor_hub_count(&hub), 0);

    sensor_hub_register(&hub, "a", "", mock_temp_read, NULL);
    ASSERT_EQ(sensor_hub_count(&hub), 1);

    sensor_hub_register(&hub, "b", "", mock_hum_read, NULL);
    ASSERT_EQ(sensor_hub_count(&hub), 2);
}

TEST(test_register_null_params)
{
    setup();
    ASSERT_EQ(sensor_hub_register(NULL, "x", "", mock_temp_read, NULL),
              SENSOR_HUB_ERR_PARAM);
    ASSERT_EQ(sensor_hub_register(&hub, NULL, "", mock_temp_read, NULL),
              SENSOR_HUB_ERR_PARAM);
    ASSERT_EQ(sensor_hub_register(&hub, "x", "", NULL, NULL),
              SENSOR_HUB_ERR_PARAM);
}

TEST(test_initial_status_stale)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    float val;
    sensor_hub_status_t st = SENSOR_HUB_STATUS_OK;
    sensor_hub_get(&hub, 0, &val, &st);
    ASSERT_EQ(st, SENSOR_HUB_STATUS_STALE);
}

TEST(test_ctx_passed_to_read)
{
    setup();
    int flag = 0;
    sensor_hub_register(&hub, "ctx_sensor", "", mock_ctx_read, &flag);
    sensor_hub_poll(&hub);
    ASSERT_TRUE(ctx_read_called);
    ASSERT_EQ(flag, 42);
}

TEST(test_error_then_recovery)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    /* first poll: error */
    mock_temp_err = SENSOR_HUB_ERR_READ;
    sensor_hub_poll(&hub);
    sensor_hub_status_t st;
    sensor_hub_get(&hub, 0, NULL, &st);
    ASSERT_EQ(st, SENSOR_HUB_STATUS_ERROR);

    /* second poll: recovery */
    mock_temp_err = SENSOR_HUB_OK;
    mock_temp_value = 23.0f;
    sensor_hub_poll(&hub);
    float val;
    sensor_hub_get(&hub, 0, &val, &st);
    ASSERT_EQ(st, SENSOR_HUB_STATUS_OK);
    ASSERT_NEAR(val, 23.0f, 0.001f);
}

TEST(test_poll_empty_hub)
{
    setup();
    /* polling with no sensors should succeed (no-op) */
    sensor_hub_err_t err = sensor_hub_poll(&hub);
    ASSERT_EQ(err, SENSOR_HUB_OK);
}

TEST(test_get_stats_partial_null_out)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    mock_temp_value = 15.0f;
    sensor_hub_poll(&hub);

    /* call with some output pointers NULL -- should not crash */
    float min;
    sensor_hub_err_t err = sensor_hub_get_stats(&hub, 0, &min, NULL, NULL);
    ASSERT_EQ(err, SENSOR_HUB_OK);
    ASSERT_NEAR(min, 15.0f, 0.001f);
}

TEST(test_get_value_null_out)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    sensor_hub_poll(&hub);

    /* call with NULL value/status -- should not crash */
    sensor_hub_err_t err = sensor_hub_get(&hub, 0, NULL, NULL);
    ASSERT_EQ(err, SENSOR_HUB_OK);
}

TEST(test_unit_stored)
{
    setup();
    sensor_hub_register(&hub, "temp", "degC", mock_temp_read, NULL);
    ASSERT_TRUE(strcmp(hub.sensors[0].unit, "degC") == 0);
}

/* The header's rule, pinned: name and unit are kept by pointer. */
TEST(test_name_and_unit_are_kept_by_pointer)
{
    char name[] = "old";
    char unit[] = "C";

    setup();
    sensor_hub_register(&hub, name, unit, mock_temp_read, NULL);
    ASSERT_TRUE(hub.sensors[0].unit == unit);
    ASSERT_EQ(sensor_hub_find(&hub, "old"), 0);

    memcpy(name, "new", sizeof(name));
    ASSERT_EQ(sensor_hub_find(&hub, "old"), -1);
    ASSERT_EQ(sensor_hub_find(&hub, "new"), 0);
}

/* ================================================================ */

/* One reading that is not a number must not reach the statistics, where only
 * sensor_hub_reset_stats could get it out again. */
TEST(test_non_finite_reading_is_a_read_error)
{
    static const float bad[] = { INFINITY, -INFINITY, NAN };

    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);
    mock_temp_value = 100.0f;
    ASSERT_EQ(sensor_hub_poll(&hub), SENSOR_HUB_OK);

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        float value = 0.0f;
        sensor_hub_status_t status = SENSOR_HUB_STATUS_OK;

        mock_temp_value = bad[i];
        ASSERT_EQ(sensor_hub_poll(&hub), SENSOR_HUB_ERR_READ);
        ASSERT_EQ(sensor_hub_get(&hub, 0, &value, &status), SENSOR_HUB_OK);
        ASSERT_EQ(status, SENSOR_HUB_STATUS_ERROR);
        ASSERT_NEAR(value, 100.0f, 0.001f);
        ASSERT_EQ(hub.sensors[0].error_count, i + 1u);
    }

    mock_temp_value = 100.0f;
    ASSERT_EQ(sensor_hub_poll(&hub), SENSOR_HUB_OK);

    float min = 0.0f, max = 0.0f, avg = 0.0f;
    ASSERT_EQ(sensor_hub_get_stats(&hub, 0, &min, &max, &avg), SENSOR_HUB_OK);
    ASSERT_NEAR(min, 100.0f, 0.001f);
    ASSERT_NEAR(max, 100.0f, 0.001f);
    ASSERT_NEAR(avg, 100.0f, 0.001f);
}

/* The largest floats are still readings. */
TEST(test_largest_finite_readings_are_accepted)
{
    setup();
    sensor_hub_register(&hub, "temp", "C", mock_temp_read, NULL);

    mock_temp_value = FLT_MAX;
    ASSERT_EQ(sensor_hub_poll(&hub), SENSOR_HUB_OK);
    mock_temp_value = -FLT_MAX;
    ASSERT_EQ(sensor_hub_poll(&hub), SENSOR_HUB_OK);
    ASSERT_EQ(hub.sensors[0].error_count, 0);
}

TEST_MAIN("sensor_hub")
    RUN(test_init_valid);
    RUN(test_init_null);
    RUN(test_register_and_poll);
    RUN(test_register_max_sensors_full);
    RUN(test_poll_sensor_error);
    RUN(test_stats_min_max_avg);
    RUN(test_reset_stats);
    RUN(test_find_by_name);
    RUN(test_find_null_params);
    RUN(test_multiple_sensors_polled);
    RUN(test_null_hub_params);
    RUN(test_get_invalid_index);
    RUN(test_varying_values_stats);
    RUN(test_zero_readings_avg);
    RUN(test_sensor_count);
    RUN(test_register_null_params);
    RUN(test_initial_status_stale);
    RUN(test_ctx_passed_to_read);
    RUN(test_error_then_recovery);
    RUN(test_poll_empty_hub);
    RUN(test_get_stats_partial_null_out);
    RUN(test_get_value_null_out);
    RUN(test_unit_stored);
    RUN(test_name_and_unit_are_kept_by_pointer);
    RUN(test_non_finite_reading_is_a_read_error);
    RUN(test_largest_finite_readings_are_accepted);
TEST_MAIN_END()
