// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_shtc3.h"
#include "shtc3.h"

static const shtc3_i2c_t hw_i2c = {
    .write    = hw_model_write,
    .read     = hw_model_read,
    .delay_ms = hw_model_delay,
    .ctx      = NULL,
};

static shtc3_t dev;

/* Reach the sensor directly, for what the driver must not do */
static bool raw_cmd(uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return hw_model_write(NULL, SHTC3_ADDR, cmd, 2);
}

static bool raw_read(uint16_t len)
{
    uint8_t buf[6];
    return hw_model_read(NULL, SHTC3_ADDR, buf, len);
}

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    shtc3_err_t e = shtc3_init(&dev, &hw_i2c);
    ASSERT_EQ(e, SHTC3_OK);
    ASSERT_TRUE(hw_read_count > 0);
    /* After init, sensor should be asleep */
    ASSERT_FALSE(hw_awake);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    hw_model_inject_reading(26214, 32768);
    shtc3_init(&dev, &hw_i2c);

    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.1f);
    /* After read, sensor should be asleep again */
    ASSERT_FALSE(hw_awake);
}

TEST(test_hw_wakeup_sleep_state)
{
    hw_model_reset();
    shtc3_init(&dev, &hw_i2c);

    /* After init, sensor is asleep */
    ASSERT_FALSE(hw_awake);

    shtc3_wakeup(&dev);
    ASSERT_TRUE(hw_awake);

    shtc3_sleep(&dev);
    ASSERT_FALSE(hw_awake);
}

/* p.4: "After power-up the sensor is in the idle state unless it is sent to sleep
 * mode or other data transmission is active." */
TEST(test_hw_model_powers_up_idle)
{
    hw_model_reset();
    ASSERT_TRUE(hw_awake);
    ASSERT_TRUE(raw_cmd(0xEF, 0xC8));
    ASSERT_TRUE(raw_read(3));
}

/* p.7: "When the sensor is in sleep mode, it requires the following wake up
 * command before any further communication" */
TEST(test_hw_model_asleep_takes_only_the_wakeup)
{
    hw_model_reset();
    ASSERT_TRUE(raw_cmd(0xB0, 0x98));
    ASSERT_FALSE(hw_awake);

    ASSERT_FALSE(raw_cmd(0xEF, 0xC8));
    ASSERT_FALSE(raw_cmd(0x78, 0x66));
    ASSERT_FALSE(raw_cmd(0x80, 0x5D));
    ASSERT_FALSE(raw_read(3));
    ASSERT_EQ(hw_reset_count, 0);

    ASSERT_TRUE(raw_cmd(0x35, 0x17));
    ASSERT_TRUE(hw_awake);
}

/* p.9: "In general, the sensor does not respond to any I2C activity during
 * measurement, i.e. I2C read and write headers are not acknowledged (NACK)."
 * Table 5, p.5: tMEAS is 10.8 ms. */
TEST(test_hw_model_silent_during_a_measurement)
{
    hw_model_reset();
    ASSERT_TRUE(raw_cmd(0x78, 0x66));

    hw_model_advance_us(10799);
    ASSERT_FALSE(raw_read(6));
    ASSERT_FALSE(raw_cmd(0xEF, 0xC8));

    hw_model_advance_us(1);
    ASSERT_TRUE(raw_read(6));
}

/* Figure 7: the first command after the Wakeup comes after "Wakeup time see Table
 * 5", which has 230 us as tPU. The manual does not say what a command inside that
 * time meets, so the model takes the strict reading. */
TEST(test_hw_model_needs_the_wakeup_time)
{
    hw_model_reset();
    ASSERT_TRUE(raw_cmd(0xB0, 0x98));
    ASSERT_TRUE(raw_cmd(0x35, 0x17));

    hw_model_advance_us(229);
    ASSERT_FALSE(raw_cmd(0xEF, 0xC8));
    hw_model_advance_us(1);
    ASSERT_TRUE(raw_cmd(0xEF, 0xC8));
}

/* Table 5, p.5: tSR, 173 us, "Time between ACK of soft reset command and sensor
 * entering idle state". p.9: it only works when no measurement is in progress. */
TEST(test_hw_model_needs_the_reset_time)
{
    hw_model_reset();
    ASSERT_TRUE(raw_cmd(0x80, 0x5D));
    ASSERT_EQ(hw_reset_count, 1);

    hw_model_advance_us(172);
    ASSERT_FALSE(raw_cmd(0xEF, 0xC8));
    hw_model_advance_us(1);
    ASSERT_TRUE(raw_cmd(0xEF, 0xC8));
}

/* A read header with no command before it has nothing to answer */
TEST(test_hw_model_read_without_a_command_is_nacked)
{
    hw_model_reset();
    ASSERT_FALSE(raw_read(3));
}

/* p.9: the driver uses no clock stretching, so it must wait out the measurement */
TEST(test_hw_read_waits_out_the_measurement)
{
    hw_model_reset();
    ASSERT_EQ(shtc3_init(&dev, &hw_i2c), SHTC3_OK);

    uint32_t start = hw_now_us;
    shtc3_data_t data;
    ASSERT_EQ(shtc3_read(&dev, &data), SHTC3_OK);
    ASSERT_GE(hw_now_us - start, 10800u);
}

/* Reads run back to back, each from sleep */
TEST(test_hw_reads_in_a_row)
{
    hw_model_reset();
    ASSERT_EQ(shtc3_init(&dev, &hw_i2c), SHTC3_OK);

    for (int i = 0; i < 3; i++)
    {
        shtc3_data_t data;
        ASSERT_EQ(shtc3_read(&dev, &data), SHTC3_OK);
        ASSERT_FALSE(hw_awake);
    }
}

/* Init and read both leave the sensor asleep, and p.7 lets nothing but the Wakeup
 * through then, so a reset has to wake it first */
TEST(test_hw_reset_reaches_a_sleeping_sensor)
{
    hw_model_reset();
    ASSERT_EQ(shtc3_init(&dev, &hw_i2c), SHTC3_OK);
    ASSERT_FALSE(hw_awake);

    ASSERT_EQ(shtc3_reset(&dev), SHTC3_OK);
    ASSERT_EQ(hw_reset_count, 1);
}

/* After shtc3_wakeup and shtc3_reset the next command must find the sensor idle */
TEST(test_hw_sensor_is_idle_when_the_call_returns)
{
    hw_model_reset();
    ASSERT_EQ(shtc3_init(&dev, &hw_i2c), SHTC3_OK);

    ASSERT_EQ(shtc3_wakeup(&dev), SHTC3_OK);
    ASSERT_TRUE(raw_cmd(0xEF, 0xC8));

    ASSERT_EQ(shtc3_reset(&dev), SHTC3_OK);
    ASSERT_TRUE(raw_cmd(0xEF, 0xC8));
}

TEST_MAIN("shtc3_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_wakeup_sleep_state);
    RUN(test_hw_model_powers_up_idle);
    RUN(test_hw_model_asleep_takes_only_the_wakeup);
    RUN(test_hw_model_silent_during_a_measurement);
    RUN(test_hw_model_needs_the_wakeup_time);
    RUN(test_hw_model_needs_the_reset_time);
    RUN(test_hw_model_read_without_a_command_is_nacked);
    RUN(test_hw_read_waits_out_the_measurement);
    RUN(test_hw_reads_in_a_row);
    RUN(test_hw_reset_reaches_a_sleeping_sensor);
    RUN(test_hw_sensor_is_idle_when_the_call_returns);
TEST_MAIN_END()
