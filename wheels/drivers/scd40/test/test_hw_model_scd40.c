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

#define SIGNAL_UPDATE_MS SCD40_MODEL_INTERVAL_MS

/* Start a measurement and let the first sample arrive */
static void measure(void)
{
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    scd40_model_delay(&model, SIGNAL_UPDATE_MS);
}

/* Reach the sensor directly, for what the driver must not do */
static bool raw_cmd(uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return scd40_model_write(&model, SCD40_DEV_ADDR, cmd, 2);
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
    measure();

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
    measure();

    scd40_data_t data;
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
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
    measure();

    scd40_data_t data;
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
    ASSERT_NEAR(data.temperature_c, -10.0f, 1.0f);
    ASSERT_NEAR(data.humidity_pct, 95.0f, 0.5f);
}

TEST(test_hw_measure_count)
{
    setup();
    measure();
    scd40_data_t data;
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
    ASSERT_EQ(model.measure_count, 1);
    scd40_model_delay(&model, SIGNAL_UPDATE_MS);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
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

/* p.17: reinit works "by reloading user settings from EEPROM", and the driver has
 * no persist_settings, so the altitude does not survive it */
TEST(test_hw_reinit_reloads_the_saved_altitude)
{
    setup();
    ASSERT_EQ(scd40_set_altitude(&dev, 2000), SCD40_OK);
    ASSERT_EQ(scd40_perform_reset(&dev), SCD40_OK);
    ASSERT_EQ(model.altitude_m, 0);
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
    measure();
    scd40_data_t d1, d2;
    ASSERT_EQ(scd40_read(&dev, &d1), SCD40_OK);
    scd40_model_delay(&model, SIGNAL_UPDATE_MS);
    ASSERT_EQ(scd40_read(&dev, &d2), SCD40_OK);
    ASSERT_EQ(d1.co2_ppm, d2.co2_ppm);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.humidity_pct, d2.humidity_pct, 0.01f);
}

/* --- what the sensor refuses ------------------------------------------- */

/* p.9: "The measurement data can only be read out once per signal update interval
 * as the buffer is emptied upon read-out. If no data is available in the buffer,
 * the sensor returns a NACK." */
TEST(test_hw_read_empties_the_buffer)
{
    setup();
    measure();

    scd40_data_t data;
    bool ready = false;
    ASSERT_EQ(scd40_data_ready(&dev, &ready), SCD40_OK);
    ASSERT_TRUE(ready);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);

    ASSERT_EQ(scd40_data_ready(&dev, &ready), SCD40_OK);
    ASSERT_FALSE(ready);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_ERR_I2C);

    scd40_model_delay(&model, SIGNAL_UPDATE_MS);
    ASSERT_EQ(scd40_data_ready(&dev, &ready), SCD40_OK);
    ASSERT_TRUE(ready);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
}

/* Nothing has been measured yet after power-up, or before the first interval */
TEST(test_hw_no_data_before_the_first_interval)
{
    setup();
    scd40_data_t data;
    bool ready = true;

    ASSERT_EQ(scd40_read(&dev, &data), SCD40_ERR_I2C);

    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    scd40_model_delay(&model, SIGNAL_UPDATE_MS - 10u);
    ASSERT_EQ(scd40_data_ready(&dev, &ready), SCD40_OK);
    ASSERT_FALSE(ready);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_ERR_I2C);
}

/* p.7: "the sensor needs the execution time (see Table 9) to respond to the I2C
 * read header with an ACK bit" */
TEST(test_hw_read_header_needs_the_execution_time)
{
    setup();
    uint8_t buf[9];

    ASSERT_TRUE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
    ASSERT_FALSE(scd40_model_read(&model, SCD40_DEV_ADDR, buf, sizeof(buf)));
    scd40_model_delay(&model, 1);
    ASSERT_TRUE(scd40_model_read(&model, SCD40_DEV_ADDR, buf, sizeof(buf)));
}

/* A read with no command before it has nothing to answer */
TEST(test_hw_read_without_a_command_is_nacked)
{
    setup();
    uint8_t buf[3];
    ASSERT_FALSE(scd40_model_read(&model, SCD40_DEV_ADDR, buf, sizeof(buf)));
}

/* p.7: "Commands must not be sent while a preceding command is being processed."
 * p.10: "the sensor will only respond to other commands 500 ms after the
 * stop_periodic_measurement command has been issued." */
TEST(test_hw_silent_for_500_ms_after_stop)
{
    setup();
    ASSERT_TRUE(raw_cmd(SCD40_CMD_START_PERIODIC_MSB, SCD40_CMD_START_PERIODIC_LSB));
    ASSERT_TRUE(raw_cmd(SCD40_CMD_STOP_PERIODIC_MSB, SCD40_CMD_STOP_PERIODIC_LSB));

    ASSERT_FALSE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
    scd40_model_delay(&model, 499);
    ASSERT_FALSE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
    scd40_model_delay(&model, 1);
    ASSERT_TRUE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
}

/* Table 9, p.8: reinit takes 30 ms and set_sensor_altitude 1 ms */
TEST(test_hw_busy_for_the_execution_time)
{
    setup();
    ASSERT_TRUE(raw_cmd(SCD40_CMD_REINIT_MSB, SCD40_CMD_REINIT_LSB));
    scd40_model_delay(&model, 29);
    ASSERT_FALSE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
    scd40_model_delay(&model, 1);
    ASSERT_TRUE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
}

/* The driver's own waits are long enough for every command in a row */
TEST(test_hw_driver_waits_out_each_command)
{
    setup();
    uint16_t serial[3];
    ASSERT_EQ(scd40_get_serial(&dev, serial), SCD40_OK);
    ASSERT_EQ(scd40_set_altitude(&dev, 500), SCD40_OK);
    ASSERT_EQ(scd40_perform_reset(&dev), SCD40_OK);
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    ASSERT_EQ(scd40_stop_periodic(&dev), SCD40_OK);
    ASSERT_EQ(scd40_get_serial(&dev, serial), SCD40_OK);
}

/* p.9: "While a periodic measurement is running, no other commands must be issued
 * with the exception of read_measurement, get_data_ready_status,
 * stop_periodic_measurement, set_ambient_pressure and get_ambient_pressure." */
TEST(test_hw_idle_commands_nacked_during_a_measurement)
{
    setup();
    ASSERT_TRUE(raw_cmd(SCD40_CMD_START_PERIODIC_MSB, SCD40_CMD_START_PERIODIC_LSB));

    ASSERT_FALSE(raw_cmd(SCD40_CMD_GET_SERIAL_MSB, SCD40_CMD_GET_SERIAL_LSB));
    ASSERT_FALSE(raw_cmd(SCD40_CMD_REINIT_MSB, SCD40_CMD_REINIT_LSB));
    ASSERT_FALSE(raw_cmd(SCD40_CMD_START_PERIODIC_MSB, SCD40_CMD_START_PERIODIC_LSB));
    ASSERT_EQ(model.reset_count, 0);
    ASSERT_EQ(model.start_count, 1);
    ASSERT_EQ(model.forbidden_cmds, 3);

    uint8_t alt[5] = { SCD40_CMD_SET_ALTITUDE_MSB, SCD40_CMD_SET_ALTITUDE_LSB, 0x01, 0xF4, 0 };
    alt[4] = scd40_model_crc8(&alt[2], 2);
    ASSERT_FALSE(scd40_model_write(&model, SCD40_DEV_ADDR, alt, sizeof(alt)));
    ASSERT_EQ(model.altitude_m, 0);

    /* The allowed ones still work */
    ASSERT_TRUE(raw_cmd(SCD40_CMD_GET_DATA_READY_MSB, SCD40_CMD_GET_DATA_READY_LSB));
    scd40_model_delay(&model, 1);
    ASSERT_TRUE(raw_cmd(SCD40_CMD_READ_MEASUREMENT_MSB, SCD40_CMD_READ_MEASUREMENT_LSB));
    scd40_model_delay(&model, 1);
    ASSERT_TRUE(raw_cmd(SCD40_CMD_STOP_PERIODIC_MSB, SCD40_CMD_STOP_PERIODIC_LSB));
}

/* The driver keeps those commands away from a running measurement */
TEST(test_hw_driver_sends_no_idle_command_during_a_measurement)
{
    setup();
    uint16_t serial[3];

    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    ASSERT_EQ(scd40_get_serial(&dev, serial), SCD40_ERR_BUSY);
    ASSERT_EQ(scd40_set_altitude(&dev, 100), SCD40_ERR_BUSY);
    ASSERT_EQ(scd40_perform_reset(&dev), SCD40_ERR_BUSY);
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_ERR_BUSY);
    ASSERT_EQ(model.forbidden_cmds, 0);
    ASSERT_EQ(model.altitude_m, 0);
    ASSERT_EQ(model.start_count, 1);

    /* Back in idle mode they all work again */
    ASSERT_EQ(scd40_stop_periodic(&dev), SCD40_OK);
    ASSERT_EQ(scd40_get_serial(&dev, serial), SCD40_OK);
    ASSERT_EQ(scd40_set_altitude(&dev, 100), SCD40_OK);
    ASSERT_EQ(scd40_perform_reset(&dev), SCD40_OK);
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_OK);
    ASSERT_EQ(model.forbidden_cmds, 0);
}

/* The commands Table 9 allows go through while it runs */
TEST(test_hw_driver_reads_during_a_measurement)
{
    setup();
    measure();

    bool ready = false;
    scd40_data_t data;
    ASSERT_EQ(scd40_data_ready(&dev, &ready), SCD40_OK);
    ASSERT_TRUE(ready);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_OK);
    ASSERT_EQ(model.forbidden_cmds, 0);
}

/* p.7: "In write direction it is mandatory to transmit the checksum." */
TEST(test_hw_altitude_needs_a_good_checksum)
{
    setup();
    uint8_t alt[5] = { SCD40_CMD_SET_ALTITUDE_MSB, SCD40_CMD_SET_ALTITUDE_LSB, 0x07, 0x9E, 0x00 };

    alt[4] = (uint8_t)(scd40_model_crc8(&alt[2], 2) ^ 0xFFu);
    ASSERT_FALSE(scd40_model_write(&model, SCD40_DEV_ADDR, alt, sizeof(alt)));
    ASSERT_EQ(model.altitude_m, 0);

    /* Without the CRC byte it is no write sequence either */
    ASSERT_FALSE(scd40_model_write(&model, SCD40_DEV_ADDR, alt, 4));
    ASSERT_EQ(model.altitude_m, 0);

    alt[4] = scd40_model_crc8(&alt[2], 2);
    ASSERT_TRUE(scd40_model_write(&model, SCD40_DEV_ADDR, alt, sizeof(alt)));
    ASSERT_EQ(model.altitude_m, 1950);
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
    RUN(test_hw_reinit_reloads_the_saved_altitude);
    RUN(test_hw_crc_matches_driver);
    RUN(test_hw_multiple_reads_consistent);
    RUN(test_hw_read_empties_the_buffer);
    RUN(test_hw_no_data_before_the_first_interval);
    RUN(test_hw_read_header_needs_the_execution_time);
    RUN(test_hw_read_without_a_command_is_nacked);
    RUN(test_hw_silent_for_500_ms_after_stop);
    RUN(test_hw_busy_for_the_execution_time);
    RUN(test_hw_driver_waits_out_each_command);
    RUN(test_hw_idle_commands_nacked_during_a_measurement);
    RUN(test_hw_driver_sends_no_idle_command_during_a_measurement);
    RUN(test_hw_driver_reads_during_a_measurement);
    RUN(test_hw_altitude_needs_a_good_checksum);
TEST_MAIN_END()
