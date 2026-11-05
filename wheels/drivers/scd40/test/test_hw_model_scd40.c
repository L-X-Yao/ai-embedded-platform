// SPDX-License-Identifier: MIT
/*
 * test_hw_model_scd40.c — tests using command-based hardware model
 *
 * Validates SCD40 driver against a command-response model.
 * Exercises measurement, periodic control, serial read, altitude set,
 * data ready, reset, CRC validation, and varying environment.
 */

#include "test_harness.h"
#include "hw_model_scd40.h"
#include "../scd40.h"
#include <math.h>

static scd40_model_t model;

static const scd40_i2c_t model_i2c = {
    .write    = scd40_model_write,
    .read     = scd40_model_read,
    .delay_ms = scd40_model_delay,
    .ctx      = &model,
};

static scd40_t dev;

static void setup(void)
{
    scd40_model_init(&model);
    scd40_init(&dev, &model_i2c);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_ok)
{
    setup();
    scd40_t d;
    scd40_err_t err = scd40_init(&d, &model_i2c);
    ASSERT_EQ(err, SCD40_OK);
}

TEST(test_hw_init_null)
{
    setup();
    scd40_t d;
    ASSERT_EQ(scd40_init(&d, NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_init(NULL, &model_i2c), SCD40_ERR_PARAM);
}

/* --- periodic control -------------------------------------------------- */

TEST(test_hw_start_stop_periodic)
{
    setup();
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    ASSERT_EQ(model.start_count, 1);
    ASSERT_TRUE(model.periodic_running);

    ASSERT_EQ(scd40_stop_periodic(&dev), SCD40_OK);
    ASSERT_EQ(model.stop_count, 1);
    ASSERT_FALSE(model.periodic_running);
}

/* --- measurement ------------------------------------------------------- */

TEST(test_hw_read_25c_50rh_415ppm)
{
    setup();
    model.true_co2_ppm      = 415;
    model.true_temp_c       = 25.0f;
    model.true_humidity_pct = 50.0f;

    scd40_data_t data;
    scd40_err_t err = scd40_read(&dev, &data);
    ASSERT_EQ(err, SCD40_OK);
    ASSERT_EQ(data.co2_ppm, 415);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.5f);
}

TEST(test_hw_read_hot_dry_high_co2)
{
    setup();
    model.true_co2_ppm      = 5000;
    model.true_temp_c       = 60.0f;
    model.true_humidity_pct = 10.0f;

    scd40_data_t data;
    scd40_read(&dev, &data);
    ASSERT_EQ(data.co2_ppm, 5000);
    ASSERT_NEAR(data.temperature_c, 60.0f, 1.0f);
    ASSERT_NEAR(data.humidity_pct, 10.0f, 0.5f);
}

TEST(test_hw_read_cold_humid)
{
    setup();
    model.true_co2_ppm      = 200;
    model.true_temp_c       = -10.0f;
    model.true_humidity_pct = 95.0f;

    scd40_data_t data;
    scd40_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, -10.0f, 1.0f);
    ASSERT_NEAR(data.humidity_pct, 95.0f, 0.5f);
}

TEST(test_hw_measure_count)
{
    setup();
    scd40_data_t data;
    scd40_read(&dev, &data);
    ASSERT_EQ(model.measure_count, 1);
    scd40_read(&dev, &data);
    ASSERT_EQ(model.measure_count, 2);
}

/* --- data ready -------------------------------------------------------- */

TEST(test_hw_data_ready)
{
    setup();
    bool ready = false;

    model.data_ready = false;
    scd40_data_ready(&dev, &ready);
    ASSERT_FALSE(ready);

    model.data_ready = true;
    scd40_data_ready(&dev, &ready);
    ASSERT_TRUE(ready);
}

/* --- serial number ----------------------------------------------------- */

TEST(test_hw_get_serial)
{
    setup();
    uint16_t serial[3] = {0};
    scd40_err_t err = scd40_get_serial(&dev, serial);
    ASSERT_EQ(err, SCD40_OK);
    ASSERT_EQ_HEX(serial[0], 0xBEEF);
    ASSERT_EQ_HEX(serial[1], 0xCAFE);
    ASSERT_EQ_HEX(serial[2], 0xDEAD);
}

/* --- altitude ---------------------------------------------------------- */

TEST(test_hw_set_altitude)
{
    setup();
    scd40_err_t err = scd40_set_altitude(&dev, 2000);
    ASSERT_EQ(err, SCD40_OK);
    ASSERT_EQ(model.altitude_m, 2000);
}

/* --- reset ------------------------------------------------------------- */

TEST(test_hw_perform_reset)
{
    setup();
    scd40_start_periodic(&dev);
    ASSERT_TRUE(model.periodic_running);

    /* p17: stop_periodic_measurement must come before reinit */
    ASSERT_EQ(scd40_stop_periodic(&dev), SCD40_OK);
    scd40_err_t err = scd40_perform_reset(&dev);
    ASSERT_EQ(err, SCD40_OK);
    ASSERT_EQ(model.reset_count, 1);
    ASSERT_FALSE(model.periodic_running);
}

/* --- CRC agreement ----------------------------------------------------- */

TEST(test_hw_crc_matches_driver)
{
    uint8_t data1[2] = { 0xBE, 0xEF };
    ASSERT_EQ(scd40_model_crc8(data1, 2), scd40_crc8(data1, 2));

    uint8_t data2[2] = { 0x00, 0x00 };
    ASSERT_EQ(scd40_model_crc8(data2, 2), scd40_crc8(data2, 2));

    uint8_t data3[2] = { 0xFF, 0xFF };
    ASSERT_EQ(scd40_model_crc8(data3, 2), scd40_crc8(data3, 2));
}

/* --- consistency ------------------------------------------------------- */

TEST(test_hw_multiple_reads_consistent)
{
    setup();
    scd40_data_t d1, d2;
    scd40_read(&dev, &d1);
    scd40_read(&dev, &d2);
    ASSERT_EQ(d1.co2_ppm, d2.co2_ppm);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.humidity_pct, d2.humidity_pct, 0.01f);
}

TEST_MAIN("scd40 hardware model")
    RUN(test_hw_init_ok);
    RUN(test_hw_init_null);
    RUN(test_hw_start_stop_periodic);
    RUN(test_hw_read_25c_50rh_415ppm);
    RUN(test_hw_read_hot_dry_high_co2);
    RUN(test_hw_read_cold_humid);
    RUN(test_hw_measure_count);
    RUN(test_hw_data_ready);
    RUN(test_hw_get_serial);
    RUN(test_hw_set_altitude);
    RUN(test_hw_perform_reset);
    RUN(test_hw_crc_matches_driver);
    RUN(test_hw_multiple_reads_consistent);
TEST_MAIN_END()
