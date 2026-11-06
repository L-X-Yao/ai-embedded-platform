// SPDX-License-Identifier: MIT
/*
 * test_hw_model_aht20.c — tests using the command-based hardware model
 *
 * Runs the AHT20 driver against a model that keeps its own time: the sensor
 * is not ready until the waits of the manual have passed, and status bit 7
 * stays set for as long as a measurement runs.
 */

#include "test_harness.h"
#include "hw_model_aht20.h"
#include "../aht20.h"

static aht20_model_t model;

static const aht20_i2c_t model_i2c = {
    .write    = aht20_model_write,
    .read     = aht20_model_read,
    .delay_ms = aht20_model_delay,
    .ctx      = &model,
};

static const uint8_t cmd_init[3]    = { AHT20_CMD_INIT_0, AHT20_CMD_INIT_1,
                                        AHT20_CMD_INIT_2 };
static const uint8_t cmd_trigger[3] = { AHT20_CMD_TRIGGER_0, AHT20_CMD_TRIGGER_1,
                                        AHT20_CMD_TRIGGER_2 };
static const uint8_t cmd_reset[1]   = { AHT20_CMD_SOFT_RESET };

static void setup(void)
{
    aht20_model_init(&model);
}

/* Past the 40 ms of p.8 step 1. */
static void setup_idle(void)
{
    setup();
    aht20_model_delay(&model, 40);
}

static uint8_t read_status(void)
{
    uint8_t status = 0xA5;
    (void)aht20_model_read(&model, 0x38, &status, 1);
    return status;
}

static void no_wait(void *ctx, uint16_t ms)
{
    (void)ctx;
    (void)ms;
}

/* --- what the model itself must do ------------------------------------- */

/* p.8: "Wait 40ms after power-on." p.7: until then it is not "ready to receive
 * commands sent by MCU". */
TEST(test_model_is_not_ready_after_power_on)
{
    setup();
    uint8_t status;
    ASSERT_FALSE(aht20_model_read(&model, 0x38, &status, 1));
    ASSERT_FALSE(aht20_model_write(&model, 0x38, cmd_trigger, 3));

    aht20_model_delay(&model, 39);
    ASSERT_FALSE(aht20_model_read(&model, 0x38, &status, 1));

    aht20_model_delay(&model, 1);
    ASSERT_TRUE(aht20_model_read(&model, 0x38, &status, 1));
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
}

/* p.8: "the 7-bit I2C device address 0x38" */
TEST(test_model_answers_only_at_0x38)
{
    setup_idle();
    uint8_t status;
    ASSERT_FALSE(aht20_model_read(&model, 0x39, &status, 1));
    ASSERT_FALSE(aht20_model_write(&model, 0x39, cmd_trigger, 3));
    ASSERT_EQ(model.measure_count, 0);
}

/* p.8: "Wait for 80ms to wait for the measurement to be completed. If the read
 * status word Bit [7] is 0, it indicates that the measurement is completed" */
TEST(test_model_is_busy_until_the_measurement_ends)
{
    setup_idle();
    ASSERT_EQ(read_status() & AHT20_STATUS_BUSY, 0);

    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    ASSERT_EQ_HEX(read_status() & AHT20_STATUS_BUSY, AHT20_STATUS_BUSY);

    aht20_model_delay(&model, 79);
    ASSERT_EQ_HEX(read_status() & AHT20_STATUS_BUSY, AHT20_STATUS_BUSY);

    aht20_model_delay(&model, 1);
    ASSERT_EQ(read_status() & AHT20_STATUS_BUSY, 0);
}

/* p.9: "1 represents that the sensor is busy, the host needs to wait for data
 * processing to complete." The frame stays well formed, so only bit 7 says
 * that its data is the last result and not this one. */
TEST(test_model_busy_frame_holds_the_last_result)
{
    setup_idle();
    uint8_t first[7];
    aht20_model_set_humidity(&model, 25.0f);
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    aht20_model_delay(&model, 80);
    ASSERT_TRUE(aht20_model_read(&model, 0x38, first, 7));

    uint8_t busy[7];
    aht20_model_set_humidity(&model, 75.0f);
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    aht20_model_delay(&model, 79);
    ASSERT_TRUE(aht20_model_read(&model, 0x38, busy, 7));

    ASSERT_EQ_HEX(busy[0] & AHT20_STATUS_BUSY, AHT20_STATUS_BUSY);
    ASSERT_MEMEQ_N(&busy[1], &first[1], 5);
    ASSERT_EQ_HEX(aht20_model_crc8(busy, 6), busy[6]);
}

/* p.8: "After receiving six bytes, the next byte is the CRC check data, the
 * user can read it as needed". */
TEST(test_model_short_read_gives_the_first_bytes)
{
    setup_idle();
    uint8_t frame[7];
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    aht20_model_delay(&model, 80);
    ASSERT_TRUE(aht20_model_read(&model, 0x38, frame, 7));

    for (uint16_t len = 1; len < 7u; len++) {
        uint8_t buf[7];
        memset(buf, 0xA5, sizeof(buf));
        ASSERT_TRUE(aht20_model_read(&model, 0x38, buf, len));
        ASSERT_MEMEQ_N(buf, frame, len);
        ASSERT_EQ_HEX(buf[len], 0xA5);
    }
}

/* p.8: state, two humidity bytes, the shared byte, two temperature bytes, CRC.
 * p.9: RH = SRH / 2^20 * 100 and T = ST / 2^20 * 200 - 50, so 8 / 2^20 of the
 * humidity range and 12.5 C, ST = 50000h. The manual does not say which nibble
 * of the shared byte is humidity: this is the usual split. */
TEST(test_model_frame_layout)
{
    setup_idle();
    aht20_model_set_humidity(&model, 0.000762939453125f);
    aht20_model_set_temperature(&model, 12.5f);
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    aht20_model_delay(&model, 80);

    uint8_t frame[7];
    ASSERT_TRUE(aht20_model_read(&model, 0x38, frame, 7));
    ASSERT_EQ_HEX(frame[1], 0x00);
    ASSERT_EQ_HEX(frame[2], 0x00);
    ASSERT_EQ_HEX(frame[3], 0x85);
    ASSERT_EQ_HEX(frame[4], 0x00);
    ASSERT_EQ_HEX(frame[5], 0x00);
}

/* p.9: S / 2^20 in a 20-bit field stops one step short of the top. */
TEST(test_model_raw_values_fit_20_bits)
{
    ASSERT_EQ_HEX(aht20_model_hum_to_raw(100.0f), 0xFFFFF);
    ASSERT_EQ_HEX(aht20_model_temp_to_raw(150.0f), 0xFFFFF);
    ASSERT_EQ_HEX(aht20_model_hum_to_raw(0.0f), 0x00000);
    ASSERT_EQ_HEX(aht20_model_temp_to_raw(-50.0f), 0x00000);
}

/* p.8: send 0xBE "and then wait for 10ms". */
TEST(test_model_calibrates_10ms_after_the_init_command)
{
    setup_idle();
    aht20_model_set_calibrated(&model, false);
    ASSERT_EQ(read_status() & AHT20_STATUS_CAL, 0);

    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_init, 3));
    aht20_model_delay(&model, 9);
    ASSERT_EQ(read_status() & AHT20_STATUS_CAL, 0);

    aht20_model_delay(&model, 1);
    ASSERT_EQ_HEX(read_status() & AHT20_STATUS_CAL, AHT20_STATUS_CAL);
}

/* p.9: "the time required for soft reset does not exceed 20 ms." */
TEST(test_model_is_not_ready_during_soft_reset)
{
    setup_idle();
    uint8_t status;
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_reset, 1));
    ASSERT_FALSE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    ASSERT_FALSE(aht20_model_read(&model, 0x38, &status, 1));

    aht20_model_delay(&model, 19);
    ASSERT_FALSE(aht20_model_write(&model, 0x38, cmd_trigger, 3));

    aht20_model_delay(&model, 1);
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 3));
    ASSERT_EQ(model.measure_count, 1);
}

/* Table 9: only the exact commands count. */
TEST(test_model_ignores_a_cut_short_trigger)
{
    setup_idle();
    ASSERT_TRUE(aht20_model_write(&model, 0x38, cmd_trigger, 1));
    ASSERT_EQ(model.measure_count, 0);
    ASSERT_EQ(read_status() & AHT20_STATUS_BUSY, 0);
}

/* p.8: "CRC initial value is 0XFF", polynomial 1 + x^4 + x^5 + x^8. These are
 * the parameters of the Sensirion CRC-8, whose example is CRC(0xBEEF) = 0x92. */
TEST(test_model_crc_follows_the_polynomial)
{
    const uint8_t data[2] = { 0xBE, 0xEF };
    ASSERT_EQ_HEX(aht20_model_crc8(data, 2), 0x92);
    ASSERT_EQ_HEX(aht20_crc8(data, 2), 0x92);
}

/* --- the driver against the model -------------------------------------- */

TEST(test_init_calibrated_sends_no_init_command)
{
    setup();
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);
    ASSERT_EQ(model.init_count, 0);
    ASSERT_TRUE(aht20_is_calibrated(&dev));
}

TEST(test_init_uncalibrated_calibrates)
{
    setup();
    aht20_model_set_calibrated(&model, false);
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);
    ASSERT_EQ(model.init_count, 1);
    ASSERT_TRUE(aht20_is_calibrated(&dev));
}

TEST(test_init_reports_a_failed_calibration)
{
    setup();
    aht20_model_set_calibrated(&model, false);
    aht20_model_set_calibration_fails(&model, true);
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_ERR_NOT_CAL);
}

/* The 40 ms of p.8 are real: a delay that does not delay finds no sensor. */
TEST(test_init_needs_the_power_on_wait)
{
    setup();
    aht20_i2c_t hal = model_i2c;
    hal.delay_ms = no_wait;
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &hal), AHT20_ERR_I2C);
}

TEST(test_read_values)
{
    static const struct {
        float temp_c;
        float hum_pct;
    } rows[] = {
        {  25.0f,  50.0f },
        { -40.0f,   0.0f },
        {  85.0f, 100.0f },
        {   0.0f,  12.5f },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup();
        aht20_model_set_temperature(&model, rows[i].temp_c);
        aht20_model_set_humidity(&model, rows[i].hum_pct);
        aht20_t dev;
        ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);

        aht20_data_t data;
        ASSERT_EQ(aht20_read(&dev, &data), AHT20_OK);
        /* one step of the 20-bit field is below 0.0002 in either unit */
        ASSERT_NEAR(data.temperature_c, rows[i].temp_c, 0.001f);
        ASSERT_NEAR(data.humidity_pct, rows[i].hum_pct, 0.001f);
        ASSERT_EQ(model.measure_count, 1);
    }
}

/* The 80 ms of p.8 are real: with no wait the sensor stays busy, and the old
 * data with a good CRC must not come back as a new result. */
TEST(test_read_needs_the_measurement_wait)
{
    setup();
    aht20_i2c_t hal = model_i2c;
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &hal), AHT20_OK);

    hal.delay_ms = no_wait;
    aht20_data_t data;
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_ERR_TIMEOUT);
}

/* p.8: "otherwise, continue to wait." */
TEST(test_read_waits_for_a_slow_measurement)
{
    setup();
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);

    aht20_model_set_measure_ms(&model, 150);
    aht20_model_set_temperature(&model, 40.0f);
    aht20_data_t data;
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_OK);
    ASSERT_NEAR(data.temperature_c, 40.0f, 0.001f);
}

TEST(test_read_gives_up_on_a_stuck_sensor)
{
    setup();
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);

    aht20_model_set_measure_ms(&model, 60000);
    aht20_data_t data;
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_ERR_TIMEOUT);
}

TEST(test_each_read_is_a_new_measurement)
{
    setup();
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);

    aht20_data_t data;
    aht20_model_set_temperature(&model, 10.0f);
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_OK);
    ASSERT_NEAR(data.temperature_c, 10.0f, 0.001f);

    aht20_model_set_temperature(&model, 30.0f);
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_OK);
    ASSERT_NEAR(data.temperature_c, 30.0f, 0.001f);
    ASSERT_EQ(model.measure_count, 2);
}

/* p.9: the sensor needs up to 20 ms after the command. */
TEST(test_read_right_after_reset)
{
    setup();
    aht20_t dev;
    ASSERT_EQ(aht20_init(&dev, &model_i2c), AHT20_OK);
    ASSERT_EQ(aht20_reset(&dev), AHT20_OK);
    ASSERT_EQ(model.reset_count, 1);

    aht20_data_t data;
    ASSERT_EQ(aht20_read(&dev, &data), AHT20_OK);
}

TEST_MAIN("aht20 hardware model")
    RUN(test_model_is_not_ready_after_power_on);
    RUN(test_model_answers_only_at_0x38);
    RUN(test_model_is_busy_until_the_measurement_ends);
    RUN(test_model_busy_frame_holds_the_last_result);
    RUN(test_model_short_read_gives_the_first_bytes);
    RUN(test_model_frame_layout);
    RUN(test_model_raw_values_fit_20_bits);
    RUN(test_model_calibrates_10ms_after_the_init_command);
    RUN(test_model_is_not_ready_during_soft_reset);
    RUN(test_model_ignores_a_cut_short_trigger);
    RUN(test_model_crc_follows_the_polynomial);
    RUN(test_init_calibrated_sends_no_init_command);
    RUN(test_init_uncalibrated_calibrates);
    RUN(test_init_reports_a_failed_calibration);
    RUN(test_init_needs_the_power_on_wait);
    RUN(test_read_values);
    RUN(test_read_needs_the_measurement_wait);
    RUN(test_read_waits_for_a_slow_measurement);
    RUN(test_read_gives_up_on_a_stuck_sensor);
    RUN(test_each_read_is_a_new_measurement);
    RUN(test_read_right_after_reset);
TEST_MAIN_END()
