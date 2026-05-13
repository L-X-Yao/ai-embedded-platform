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

TEST_MAIN("shtc3_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_wakeup_sleep_state);
TEST_MAIN_END()
