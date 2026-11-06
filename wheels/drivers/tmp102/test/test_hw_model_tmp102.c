// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tmp102.c — tests using hardware register model
 *
 * Validates TMP102 driver against a register model.
 * The model enforces temperature register read-only, config register
 * read-write, alert threshold registers, and shutdown/wake behavior. Results
 * come from conversions, and time passes only through tmp102_model_run_ms(),
 * so a test waits for a result the way a caller has to.
 */

#include "test_harness.h"
#include "hw_model_tmp102.h"
#include "../tmp102.h"
#include <math.h>

static tmp102_model_t model;

static const tmp102_i2c_t model_i2c = {
    .write_reg  = tmp102_model_write_reg,
    .write_regs = tmp102_model_write_regs,
    .read_regs  = tmp102_model_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static const tmp102_cfg_t default_cfg = {
    .addr = TMP102_ADDR_GND,
};

static void setup(void)
{
    tmp102_model_init(&model);
}

static void config_read(uint8_t *b1, uint8_t *b2)
{
    uint8_t buf[2] = { 0, 0 };
    model_i2c.read_regs(&model, TMP102_ADDR_GND, TMP102_REG_CONFIG, buf, 2);
    *b1 = buf[0];
    *b2 = buf[1];
}

static void config_write(uint8_t b1, uint8_t b2)
{
    uint8_t buf[2] = { b1, b2 };
    model_i2c.write_regs(&model, TMP102_ADDR_GND, TMP102_REG_CONFIG, buf, 2);
}

static uint8_t config_al(void)
{
    uint8_t b1, b2;
    config_read(&b1, &b2);
    return (uint8_t)((b2 >> 5) & 1u);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    tmp102_t dev;
    tmp102_err_t err = tmp102_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, TMP102_OK);
}

TEST(test_init_null_param)
{
    setup();
    tmp102_t dev;
    tmp102_err_t err = tmp102_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, TMP102_ERR_PARAM);
}

TEST(test_init_clears_shutdown_bit)
{
    setup();
    /* Set SD bit (bit 0 of config byte 0) before init */
    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    model.hw.regs[cfg_slot] |= 0x01;

    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    /* After init, SD bit should be cleared */
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_25c)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);
    tmp102_model_run_ms(&model, 10);

    float temp;
    tmp102_err_t err = tmp102_read_temp(&dev, &temp);
    ASSERT_EQ(err, TMP102_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_read_negative)
{
    setup();
    tmp102_model_set_temperature(&model, -10.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);
    tmp102_model_run_ms(&model, 10);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, -10.0f, 0.1f);
}

TEST(test_read_zero)
{
    setup();
    tmp102_model_set_temperature(&model, 0.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);
    tmp102_model_run_ms(&model, 10);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

TEST(test_read_high_temp)
{
    setup();
    tmp102_model_set_temperature(&model, 100.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);
    tmp102_model_run_ms(&model, 10);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 100.0f, 0.2f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_temp_register_readonly)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_model_update_temp(&model);

    uint8_t slot = TMP_SLOT(TMP102_REG_TEMP);
    uint8_t original = model.hw.regs[slot];
    hw_model__write_reg(&model.hw, slot, 0xFF);
    ASSERT_EQ(model.hw.regs[slot], original);
}

TEST(test_shutdown_sets_sd_bit)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_shutdown(&dev);

    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0x01);
}

TEST(test_wake_clears_sd_bit)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_shutdown(&dev);
    tmp102_wake(&dev);

    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0x00);
}

/* Table 6-2 (p.9): 128 C reads as 7FFh, the same as 127.9375 C */
TEST(test_read_128c_row)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_model_set_temperature(&model, 128.0f);
    tmp102_model_run_ms(&model, 10);
    float temp = 0.0f;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 127.9375f, 0.0001f);
}

TEST(test_set_alert_thresholds)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    /* Set alert high to 50 C */
    tmp102_set_alert_high(&dev, 50.0f);
    uint8_t thi_slot = TMP_SLOT(TMP102_REG_THIGH);
    /* 50 C: raw = 800, << 4 = 12800 = 0x3200 */
    ASSERT_EQ(model.hw.regs[thi_slot], 0x32);
    ASSERT_EQ(model.hw.regs[thi_slot + 1], 0x00);
}

TEST(test_different_temperatures)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_model_set_temperature(&model, 10.0f);
    tmp102_model_run_ms(&model, 10);
    float t1;
    tmp102_read_temp(&dev, &t1);

    tmp102_model_set_temperature(&model, 40.0f);
    tmp102_model_run_ms(&model, 250);
    float t2;
    tmp102_read_temp(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);
    tmp102_model_run_ms(&model, 10);

    float t1, t2;
    tmp102_read_temp(&dev, &t1);
    tmp102_read_temp(&dev, &t2);
    ASSERT_NEAR(t1, t2, 0.01f);
}

/* --- the model against the manual ------------------------------------- */

/* Configuration POR values 60h and A0h, p17 */
TEST(test_config_reset_value)
{
    setup();
    uint8_t b1, b2;
    config_read(&b1, &b2);
    ASSERT_EQ_HEX(b1, 0x60);
    ASSERT_EQ_HEX(b2, 0xA0);
}

/* "The converter resolution bits, R1 and R0, are read-only bits" (p18), and
 * AL is read-only too (p19) */
TEST(test_config_read_only_bits)
{
    setup();
    uint8_t b1, b2;
    config_write(0x00, 0x00);
    config_read(&b1, &b2);
    ASSERT_EQ_HEX(b1, 0x60);
    ASSERT_EQ_HEX(b2, 0x20);

    config_write(0x1F, 0xD0);
    config_read(&b1, &b2);
    ASSERT_EQ_HEX(b1, 0x7F);
    ASSERT_EQ_HEX(b2, 0xF0);
}

/* Bits D3 to D0 of a limit are 0 in normal mode, Tables 6-14 and 6-16 */
TEST(test_limit_low_bits_stay_zero)
{
    setup();
    uint8_t buf[2] = { 0x32, 0x0F };
    model_i2c.write_regs(&model, TMP102_ADDR_GND, TMP102_REG_THIGH, buf, 2);
    model_i2c.read_regs(&model, TMP102_ADDR_GND, TMP102_REG_THIGH, buf, 2);
    ASSERT_EQ_HEX(buf[0], 0x32);
    ASSERT_EQ_HEX(buf[1], 0x00);
}

/* "Following power-up or reset, the temperature register reads 0C until the
 * first conversion is complete" (p8), "after 10 ms (typical)" (p15) */
TEST(test_reads_zero_until_the_first_conversion)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    uint8_t buf[2] = { 0xAA, 0xAA };

    model_i2c.read_regs(&model, TMP102_ADDR_GND, TMP102_REG_TEMP, buf, 2);
    ASSERT_EQ_HEX(buf[0], 0x00);

    tmp102_model_run_ms(&model, 9);
    model_i2c.read_regs(&model, TMP102_ADDR_GND, TMP102_REG_TEMP, buf, 2);
    ASSERT_EQ_HEX(buf[0], 0x00);

    tmp102_model_run_ms(&model, 1);
    model_i2c.read_regs(&model, TMP102_ADDR_GND, TMP102_REG_TEMP, buf, 2);
    ASSERT_EQ_HEX(buf[0], 0x19);
}

/* A read does not convert: the register keeps "the output of the most
 * recent conversion", p8. Default rate 4 Hz, Table 6-5 on p15. */
TEST(test_results_follow_the_conversion_rate)
{
    setup();
    tmp102_t dev;
    ASSERT_EQ(tmp102_init(&dev, &model_i2c, &default_cfg), TMP102_OK);
    tmp102_model_run_ms(&model, 10);

    tmp102_model_set_temperature(&model, 40.0f);
    float temp = 0.0f;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 25.0f, 0.01f);

    tmp102_model_run_ms(&model, 249);
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 25.0f, 0.01f);

    tmp102_model_run_ms(&model, 1);
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 40.0f, 0.01f);
}

/* CR = 11 is 8 Hz */
TEST(test_cr_sets_the_period)
{
    setup();
    config_write(0x60, 0xC0);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(model.conversions, 1);
    tmp102_model_run_ms(&model, 125);
    ASSERT_EQ(model.conversions, 2);
}

/* The Shutdown-mode bit shuts "down all device circuitry other than the
 * serial interface", p15 */
TEST(test_shutdown_stops_the_conversions)
{
    setup();
    tmp102_t dev;
    ASSERT_EQ(tmp102_init(&dev, &model_i2c, &default_cfg), TMP102_OK);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(tmp102_shutdown(&dev), TMP102_OK);
    tmp102_model_run_ms(&model, 240);  /* to the end of this period */

    int before = model.conversions;
    tmp102_model_set_temperature(&model, 40.0f);
    tmp102_model_run_ms(&model, 5000);
    ASSERT_EQ(model.conversions, before);

    float temp = 0.0f;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);

    /* "When SD is equal to 0, the device maintains a continuous conversion
     * state", p15 */
    ASSERT_EQ(tmp102_wake(&dev), TMP102_OK);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 40.0f, 0.01f);
}

/* "the device shuts down when current conversion is completed", p15 */
TEST(test_shutdown_finishes_the_current_conversion)
{
    setup();
    tmp102_t dev;
    ASSERT_EQ(tmp102_init(&dev, &model_i2c, &default_cfg), TMP102_OK);
    tmp102_model_run_ms(&model, 5);
    ASSERT_EQ(tmp102_shutdown(&dev), TMP102_OK);
    tmp102_model_run_ms(&model, 5);
    ASSERT_EQ(model.conversions, 1);
    tmp102_model_run_ms(&model, 1000);
    ASSERT_EQ(model.conversions, 1);
}

/* "When the device is in Shutdown Mode, writing a 1 to the OS bit starts a
 * single temperature conversion. During the conversion, the OS bit reads '0'.
 * ... After the conversion, OS bit reads 1", p15 */
TEST(test_one_shot)
{
    setup();
    uint8_t b1, b2;
    config_write(0x01, 0xA0);
    tmp102_model_run_ms(&model, 1000);
    int before = model.conversions;

    config_write(0x81, 0xA0);
    config_read(&b1, &b2);
    ASSERT_EQ(b1 & 0x80, 0);
    tmp102_model_run_ms(&model, 10);
    config_read(&b1, &b2);
    ASSERT_EQ_HEX(b1 & 0x80, 0x80);
    ASSERT_EQ(model.conversions, before + 1);

    tmp102_model_run_ms(&model, 1000);
    ASSERT_EQ(model.conversions, before + 1);
}

/* With POL = 0 "the AL bit reads as 1 until the temperature equals or exceeds
 * T(HIGH) ... The AL bit continues to read as 0 until the temperature falls
 * below T(LOW)", p19 */
TEST(test_al_follows_the_limits)
{
    setup();
    tmp102_t dev;
    ASSERT_EQ(tmp102_init(&dev, &model_i2c, &default_cfg), TMP102_OK);
    ASSERT_EQ(tmp102_set_alert_high(&dev, 50.0f), TMP102_OK);
    ASSERT_EQ(tmp102_set_alert_low(&dev, 30.0f), TMP102_OK);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(config_al(), 1);

    tmp102_model_set_temperature(&model, 50.0f);
    tmp102_model_run_ms(&model, 250);
    ASSERT_EQ(config_al(), 0);

    tmp102_model_set_temperature(&model, 30.0f);
    tmp102_model_run_ms(&model, 250);
    ASSERT_EQ(config_al(), 0);

    tmp102_model_set_temperature(&model, 29.0f);
    tmp102_model_run_ms(&model, 250);
    ASSERT_EQ(config_al(), 1);
}

/* "The state of the POL bit inverts the polarity of data returned from the
 * AL bit", p19 */
TEST(test_pol_inverts_al)
{
    setup();
    config_write(0x64, 0xA0);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(config_al(), 0);
}

/* Table 6-12 on p18: F1 F0 = 01 needs two consecutive faults */
TEST(test_fault_queue_counts_conversions)
{
    setup();
    config_write(0x68, 0xA0);
    tmp102_model_set_temperature(&model, 90.0f);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(config_al(), 1);
    tmp102_model_run_ms(&model, 250);
    ASSERT_EQ(config_al(), 0);
}

/* A limit past the format's end saturates in the driver: at 25 C a limit of
 * 150 C must not read as a limit below the temperature */
TEST(test_limit_past_the_range_does_not_trip)
{
    setup();
    tmp102_t dev;
    ASSERT_EQ(tmp102_init(&dev, &model_i2c, &default_cfg), TMP102_OK);
    ASSERT_EQ(tmp102_set_alert_high(&dev, 150.0f), TMP102_OK);
    tmp102_model_run_ms(&model, 10);
    ASSERT_EQ(config_al(), 1);
}

TEST_MAIN("tmp102 hardware model")
    RUN(test_config_reset_value);
    RUN(test_config_read_only_bits);
    RUN(test_limit_low_bits_stay_zero);
    RUN(test_reads_zero_until_the_first_conversion);
    RUN(test_results_follow_the_conversion_rate);
    RUN(test_cr_sets_the_period);
    RUN(test_shutdown_stops_the_conversions);
    RUN(test_shutdown_finishes_the_current_conversion);
    RUN(test_one_shot);
    RUN(test_al_follows_the_limits);
    RUN(test_pol_inverts_al);
    RUN(test_fault_queue_counts_conversions);
    RUN(test_limit_past_the_range_does_not_trip);
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_clears_shutdown_bit);
    RUN(test_read_25c);
    RUN(test_read_negative);
    RUN(test_read_zero);
    RUN(test_read_high_temp);
    RUN(test_temp_register_readonly);
    RUN(test_shutdown_sets_sd_bit);
    RUN(test_wake_clears_sd_bit);
    RUN(test_read_128c_row);
    RUN(test_set_alert_thresholds);
    RUN(test_different_temperatures);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
