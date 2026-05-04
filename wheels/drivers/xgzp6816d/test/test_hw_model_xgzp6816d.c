/*
 * test_hw_model_xgzp6816d.c — tests using hardware model
 *
 * Validates XGZP6816D driver against a command-response model.
 * The model simulates the trigger/read protocol with configurable
 * true pressure and temperature values.
 */

#include "test_harness.h"
#include "hw_model_xgzp6816d.h"
#include "../xgzp6816d.h"
#include <math.h>

static xgzp6816d_model_t model;

static const xgzp6816d_i2c_t model_i2c = {
    .write = xgzp6816d_model_write,
    .read  = xgzp6816d_model_read,
    .ctx   = &model,
};

static void setup(void)
{
    xgzp6816d_model_init(&model);
}

/* --- probe tests ------------------------------------------------------- */

TEST(test_probe_ok)
{
    setup();
    xgzp6816d_err_t err = xgzp6816d_probe(&model_i2c);
    ASSERT_EQ(err, XGZP6816D_OK);
}

TEST(test_probe_null)
{
    xgzp6816d_err_t err = xgzp6816d_probe(NULL);
    ASSERT_EQ(err, XGZP6816D_ERR_I2C);
}

/* --- trigger tests ----------------------------------------------------- */

TEST(test_trigger_ok)
{
    setup();
    xgzp6816d_err_t err = xgzp6816d_trigger(&model_i2c);
    ASSERT_EQ(err, XGZP6816D_OK);
    ASSERT_TRUE(model.triggered);
    ASSERT_EQ(model.trigger_count, 1);
}

TEST(test_trigger_multiple)
{
    setup();
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_trigger(&model_i2c);
    ASSERT_EQ(model.trigger_count, 2);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_pressure_1atm)
{
    setup();
    xgzp6816d_model_set_pressure(&model, 101325.0f);
    xgzp6816d_trigger(&model_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_err_t err = xgzp6816d_read(&model_i2c, &data);
    ASSERT_EQ(err, XGZP6816D_OK);
    /* Round-trip error: Pa → raw → Pa, expect ~1% tolerance */
    ASSERT_NEAR(data.pressure_pa, 101325.0f, 2000.0f);
}

TEST(test_read_temperature_25c)
{
    setup();
    xgzp6816d_model_set_temperature(&model, 25.0f);
    xgzp6816d_trigger(&model_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_read(&model_i2c, &data);
    ASSERT_NEAR(data.temperature_c, 25.0f, 1.0f);
}

TEST(test_read_pressure_low)
{
    setup();
    xgzp6816d_model_set_pressure(&model, 30000.0f);
    xgzp6816d_trigger(&model_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_read(&model_i2c, &data);
    ASSERT_NEAR(data.pressure_pa, 30000.0f, 1000.0f);
}

TEST(test_read_pressure_high)
{
    setup();
    xgzp6816d_model_set_pressure(&model, 110000.0f);
    xgzp6816d_trigger(&model_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_read(&model_i2c, &data);
    ASSERT_NEAR(data.pressure_pa, 110000.0f, 1000.0f);
}

TEST(test_read_temperature_cold)
{
    setup();
    xgzp6816d_model_set_temperature(&model, -20.0f);
    xgzp6816d_trigger(&model_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_read(&model_i2c, &data);
    ASSERT_NEAR(data.temperature_c, -20.0f, 1.5f);
}

/* --- varying environment tests --------------------------------------- */

TEST(test_pressure_increases_with_value)
{
    setup();
    xgzp6816d_data_t d1, d2;

    xgzp6816d_model_set_pressure(&model, 50000.0f);
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d1);

    xgzp6816d_model_set_pressure(&model, 100000.0f);
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d2);

    ASSERT_TRUE(d2.pressure_pa > d1.pressure_pa);
}

TEST(test_temperature_increases_with_value)
{
    setup();
    xgzp6816d_data_t d1, d2;

    xgzp6816d_model_set_temperature(&model, 0.0f);
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d1);

    xgzp6816d_model_set_temperature(&model, 50.0f);
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d2);

    ASSERT_TRUE(d2.temperature_c > d1.temperature_c);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    xgzp6816d_model_set_pressure(&model, 101325.0f);
    xgzp6816d_model_set_temperature(&model, 25.0f);

    xgzp6816d_data_t d1, d2;
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d1);
    xgzp6816d_trigger(&model_i2c);
    xgzp6816d_read(&model_i2c, &d2);

    ASSERT_NEAR(d1.pressure_pa, d2.pressure_pa, 1.0f);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
}

TEST_MAIN("xgzp6816d hardware model")
    RUN(test_probe_ok);
    RUN(test_probe_null);
    RUN(test_trigger_ok);
    RUN(test_trigger_multiple);
    RUN(test_read_pressure_1atm);
    RUN(test_read_temperature_25c);
    RUN(test_read_pressure_low);
    RUN(test_read_pressure_high);
    RUN(test_read_temperature_cold);
    RUN(test_pressure_increases_with_value);
    RUN(test_temperature_increases_with_value);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
