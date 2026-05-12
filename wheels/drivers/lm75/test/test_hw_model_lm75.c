// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_lm75.h"
#include "lm75.h"

static const lm75_i2c_t hw_i2c = {
    .write_reg  = hw_model_write_reg,
    .read_regs  = hw_model_read_regs,
    .write_regs = hw_model_write_regs,
    .ctx        = NULL,
};

static lm75_t dev;

static lm75_err_t setup(void)
{
    hw_model_reset();
    return lm75_init(&dev, &hw_i2c);
}

TEST(test_hw_init_por_defaults)
{
    lm75_err_t e = setup();
    ASSERT_EQ(e, LM75_OK);
    /* POR temperature = 0 C */
    float temp = 99.0f;
    e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_hw_read_25c_round_trip)
{
    setup();
    hw_model_inject_temp(25.0f);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 25.0f, 0.5f);
}

TEST(test_hw_read_neg25c_round_trip)
{
    setup();
    hw_model_inject_temp(-25.0f);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, -25.0f, 0.5f);
}

TEST(test_hw_read_125c_max)
{
    setup();
    hw_model_inject_temp(125.0f);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 125.0f, 0.5f);
}

TEST(test_hw_read_neg55c_min)
{
    setup();
    hw_model_inject_temp(-55.0f);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, -55.0f, 0.5f);
}

TEST(test_hw_set_os_round_trip)
{
    setup();
    lm75_err_t e = lm75_set_os_temp(&dev, 80.0f);
    ASSERT_EQ(e, LM75_OK);
    /* Verify the model received the correct raw bytes (80 C = 0x5000) */
    ASSERT_EQ(lm75_mdl.tos[0], 0x50);
    ASSERT_EQ(lm75_mdl.tos[1], 0x00);
}

TEST(test_hw_set_hyst_round_trip)
{
    setup();
    lm75_err_t e = lm75_set_hyst_temp(&dev, 75.0f);
    ASSERT_EQ(e, LM75_OK);
    /* 75 C = 0x4B00 */
    ASSERT_EQ(lm75_mdl.thyst[0], 0x4B);
    ASSERT_EQ(lm75_mdl.thyst[1], 0x00);
}

TEST(test_hw_half_degree_resolution)
{
    setup();
    hw_model_inject_temp(25.5f);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 25.5f, 0.5f);
}

TEST(test_hw_init_reads_temp_reg)
{
    hw_model_reset();
    lm75_t d;
    (void)lm75_init(&d, &hw_i2c);
    /* Init must read the temperature register to verify I2C comms */
    ASSERT_TRUE(lm75_mdl.read_count >= 1);
}

TEST_MAIN("lm75_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_25c_round_trip);
    RUN(test_hw_read_neg25c_round_trip);
    RUN(test_hw_read_125c_max);
    RUN(test_hw_read_neg55c_min);
    RUN(test_hw_set_os_round_trip);
    RUN(test_hw_set_hyst_round_trip);
    RUN(test_hw_half_degree_resolution);
    RUN(test_hw_init_reads_temp_reg);
TEST_MAIN_END()
