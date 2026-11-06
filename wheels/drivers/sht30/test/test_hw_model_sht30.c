// SPDX-License-Identifier: MIT
/*
 * test_hw_model_sht30.c — tests using command-based hardware model
 *
 * Validates SHT30 driver against a command-response model.
 * The model maps 16-bit commands to 6-byte responses with CRC-8.
 * Exercises measurement, reset, CRC validation, and varying environment.
 */

#include "test_harness.h"
#include "hw_model_sht30.h"
#include "../sht30.h"
#include <math.h>

static sht30_model_t model;

static const sht30_i2c_t model_i2c = {
    .write    = sht30_model_write,
    .read     = sht30_model_read,
    .delay_ms = sht30_model_delay,
    .ctx      = &model,
};

static const sht30_cfg_t default_cfg = {
    .addr = SHT30_ADDR_A,
};

static const uint8_t cmd_measure[2] = { SHT30_CMD_SINGLE_HIGH_MSB,
                                        SHT30_CMD_SINGLE_HIGH_LSB };
static const uint8_t cmd_reset[2]   = { SHT30_CMD_SOFT_RESET_MSB,
                                        SHT30_CMD_SOFT_RESET_LSB };

static void setup(void)
{
    sht30_model_init(&model);
}

/* Past tPU: Table 5 p.7 gives 1.5 ms at most. */
static void setup_idle(void)
{
    setup();
    sht30_model_delay(&model, 2);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    sht30_t dev;
    sht30_err_t err = sht30_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, SHT30_OK);
    /* Init sends soft reset */
    ASSERT_EQ(model.reset_count, 1);
}

TEST(test_init_null_param)
{
    setup();
    sht30_t dev;
    sht30_err_t err = sht30_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, SHT30_ERR_PARAM);
}

/* The driver cannot wait out the measurement without delay_ms, and the sensor
 * NACKs the read until it ends. */
TEST(test_init_refuses_null_delay)
{
    setup();
    sht30_i2c_t no_delay = model_i2c;
    no_delay.delay_ms = NULL;
    sht30_t dev;
    ASSERT_EQ(sht30_init(&dev, &no_delay, &default_cfg), SHT30_ERR_PARAM);
    ASSERT_EQ(model.reset_count, 0);
}

/* --- read tests -------------------------------------------------------- */

/* p.9: "the sensor needs the time tPU to enter idle state. Once the idle state
 * is entered it is ready to receive commands". */
TEST(test_model_refuses_commands_during_power_up)
{
    setup();
    ASSERT_FALSE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    sht30_model_delay(&model, 1);
    ASSERT_FALSE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    sht30_model_delay(&model, 1);
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
}

/* p.12: "During the reset procedure the sensor will not process commands."
 * p.7: tSR is at most 1.5 ms after the ACK of the command. */
TEST(test_model_refuses_commands_during_soft_reset)
{
    setup_idle();
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_reset, 2));
    ASSERT_FALSE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    sht30_model_delay(&model, 1);
    ASSERT_FALSE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    sht30_model_delay(&model, 1);
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    ASSERT_EQ(model.measure_count, 1);
}

/* p.9: "After sending a command to the sensor a minimal waiting time of 1ms is
 * needed before another command can be received by the sensor." */
TEST(test_model_wants_1ms_between_commands)
{
    setup_idle();
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    ASSERT_FALSE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    ASSERT_EQ(model.measure_count, 1);

    sht30_model_delay(&model, 1);
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    ASSERT_EQ(model.measure_count, 2);
}

/* p.10: "The I2C master can abort the read transfer with a NACK condition after
 * any data byte if it is not interested in subsequent data". */
TEST(test_model_short_read_gives_the_first_bytes)
{
    setup_idle();
    uint8_t frame[6];
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
    sht30_model_delay(&model, 15);
    ASSERT_TRUE(sht30_model_read(&model, SHT30_ADDR_A, frame, 6));

    for (uint16_t len = 1; len < 6u; len++) {
        uint8_t buf[6];
        memset(buf, 0xA5, sizeof(buf));
        ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd_measure, 2));
        sht30_model_delay(&model, 15);
        ASSERT_TRUE(sht30_model_read(&model, SHT30_ADDR_A, buf, len));
        ASSERT_MEMEQ_N(buf, frame, len);
        ASSERT_EQ_HEX(buf[len], 0xA5);
    }
}

TEST(test_model_nacks_until_measurement_ends)
{
    setup_idle();
    const uint8_t *cmd = cmd_measure;
    uint8_t buf[6];

    ASSERT_FALSE(sht30_model_read(&model, SHT30_ADDR_A, buf, 6));
    ASSERT_TRUE(sht30_model_write(&model, SHT30_ADDR_A, cmd, 2));
    sht30_model_delay(&model, 14);
    ASSERT_FALSE(sht30_model_read(&model, SHT30_ADDR_A, buf, 6));
    sht30_model_delay(&model, 1);
    ASSERT_TRUE(sht30_model_read(&model, SHT30_ADDR_A, buf, 6));
    /* the data was read out, so none is present any more */
    ASSERT_FALSE(sht30_model_read(&model, SHT30_ADDR_A, buf, 6));
}

TEST(test_read_25c_50rh)
{
    setup();
    sht30_model_set_temperature(&model, 25.0f);
    sht30_model_set_humidity(&model, 50.0f);
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_data_t data;
    sht30_err_t err = sht30_read(&dev, &data);
    ASSERT_EQ(err, SHT30_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.5f);
}

TEST(test_read_hot_dry)
{
    setup();
    sht30_model_set_temperature(&model, 80.0f);
    sht30_model_set_humidity(&model, 10.0f);
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, 80.0f, 1.0f);
    ASSERT_NEAR(data.humidity_pct, 10.0f, 0.5f);
}

TEST(test_read_cold_humid)
{
    setup();
    sht30_model_set_temperature(&model, -20.0f);
    sht30_model_set_humidity(&model, 95.0f);
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, -20.0f, 1.0f);
    ASSERT_NEAR(data.humidity_pct, 95.0f, 0.5f);
}

/* --- CRC validation --------------------------------------------------- */

TEST(test_crc_matches_driver)
{
    /* Verify model CRC matches driver CRC */
    uint8_t data1[2] = { 0xBE, 0xEF };
    uint8_t model_crc = sht30_model_crc8(data1, 2);
    uint8_t driver_crc = sht30_crc8(data1, 2);
    ASSERT_EQ(model_crc, driver_crc);
    /* Table 20 p.14: "CRC (0xBEEF) = 0x92" */
    ASSERT_EQ_HEX(model_crc, 0x92);
}

TEST(test_crc_various_values)
{
    uint8_t data2[2] = { 0x00, 0x00 };
    ASSERT_EQ(sht30_model_crc8(data2, 2), sht30_crc8(data2, 2));

    uint8_t data3[2] = { 0xFF, 0xFF };
    ASSERT_EQ(sht30_model_crc8(data3, 2), sht30_crc8(data3, 2));
}

/* --- reset tests ------------------------------------------------------- */

TEST(test_reset)
{
    setup();
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_err_t err = sht30_reset(&dev);
    ASSERT_EQ(err, SHT30_OK);
    ASSERT_EQ(model.reset_count, 2); /* 1 from init + 1 explicit */
}

/* The driver cannot know how long ago power came, and the sensor needs tPU
 * and tSR before it takes the next command. */
TEST(test_read_right_after_init)
{
    setup();
    sht30_t dev;
    ASSERT_EQ(sht30_init(&dev, &model_i2c, &default_cfg), SHT30_OK);

    sht30_data_t data;
    ASSERT_EQ(sht30_read(&dev, &data), SHT30_OK);
}

TEST(test_read_right_after_reset)
{
    setup();
    sht30_t dev;
    ASSERT_EQ(sht30_init(&dev, &model_i2c, &default_cfg), SHT30_OK);
    ASSERT_EQ(sht30_reset(&dev), SHT30_OK);

    sht30_data_t data;
    ASSERT_EQ(sht30_read(&dev, &data), SHT30_OK);
}

/* --- measurement count ------------------------------------------------ */

TEST(test_read_triggers_measurement)
{
    setup();
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_EQ(model.measure_count, 1);

    sht30_read(&dev, &data);
    ASSERT_EQ(model.measure_count, 2);
}

/* --- varying environment ---------------------------------------------- */

TEST(test_temperature_varies)
{
    setup();
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);
    sht30_data_t d1, d2;

    sht30_model_set_temperature(&model, 10.0f);
    sht30_read(&dev, &d1);

    sht30_model_set_temperature(&model, 40.0f);
    sht30_read(&dev, &d2);

    ASSERT_TRUE(d2.temperature_c > d1.temperature_c);
}

TEST(test_humidity_varies)
{
    setup();
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);
    sht30_data_t d1, d2;

    sht30_model_set_humidity(&model, 20.0f);
    sht30_read(&dev, &d1);

    sht30_model_set_humidity(&model, 80.0f);
    sht30_read(&dev, &d2);

    ASSERT_TRUE(d2.humidity_pct > d1.humidity_pct);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    sht30_model_set_temperature(&model, 25.0f);
    sht30_model_set_humidity(&model, 50.0f);
    sht30_t dev;
    sht30_init(&dev, &model_i2c, &default_cfg);

    sht30_data_t d1, d2;
    sht30_read(&dev, &d1);
    sht30_read(&dev, &d2);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.humidity_pct, d2.humidity_pct, 0.01f);
}

TEST_MAIN("sht30 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_refuses_null_delay);
    RUN(test_model_refuses_commands_during_power_up);
    RUN(test_model_refuses_commands_during_soft_reset);
    RUN(test_model_wants_1ms_between_commands);
    RUN(test_model_short_read_gives_the_first_bytes);
    RUN(test_model_nacks_until_measurement_ends);
    RUN(test_read_25c_50rh);
    RUN(test_read_hot_dry);
    RUN(test_read_cold_humid);
    RUN(test_crc_matches_driver);
    RUN(test_crc_various_values);
    RUN(test_reset);
    RUN(test_read_right_after_init);
    RUN(test_read_right_after_reset);
    RUN(test_read_triggers_measurement);
    RUN(test_temperature_varies);
    RUN(test_humidity_varies);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
