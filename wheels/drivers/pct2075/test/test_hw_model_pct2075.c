// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_pct2075.h"
#include "pct2075.h"

static const pct2075_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static const pct2075_cfg_t default_cfg = {
    .addr         = PCT2075_DEV_ADDR,
    .write_config = false,
    .conf_val     = 0x00,
};

static pct2075_t dev;

static pct2075_err_t setup(void)
{
    hw_model_reset();
    return pct2075_init(&dev, &hw_i2c, &default_cfg);
}

TEST(test_hw_init_por_defaults)
{
    pct2075_err_t e = setup();
    ASSERT_EQ(e, PCT2075_OK);
    /* POR temperature = 0 C */
    float temp = 99.0f;
    e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_hw_read_25c_round_trip)
{
    setup();
    hw_model_inject_temp(25.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 25.0f, 0.125f);
}

TEST(test_hw_read_neg25c_round_trip)
{
    setup();
    hw_model_inject_temp(-25.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -25.0f, 0.125f);
}

TEST(test_hw_read_125c_max)
{
    setup();
    hw_model_inject_temp(125.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 125.0f, 0.125f);
}

TEST(test_hw_read_neg55c_min)
{
    setup();
    hw_model_inject_temp(-55.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -55.0f, 0.125f);
}

TEST(test_hw_resolution_0_125)
{
    setup();
    hw_model_inject_temp(25.125f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 25.125f, 0.125f);
}

TEST(test_hw_init_reads_temp_reg)
{
    hw_model_reset();
    pct2075_t d;
    (void)pct2075_init(&d, &hw_i2c, &default_cfg);
    /* Init must read the temperature register to verify I2C comms */
    ASSERT_TRUE(hw_read_count >= 1);
}

TEST(test_hw_config_write)
{
    hw_model_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = {
        .addr         = PCT2075_DEV_ADDR,
        .write_config = true,
        .conf_val     = 0x0C,  /* OS fault queue = 6 */
    };
    pct2075_err_t e = pct2075_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, PCT2075_OK);
    /* Config register at address 0x01 should hold 0x0C */
    ASSERT_EQ(hw_regs[0x01], 0x0C);
    ASSERT_TRUE(hw_write_count >= 1);
}

TEST(test_hw_zero_roundtrip)
{
    setup();
    hw_model_inject_temp(0.0f);
    float temp = 99.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_hw_neg0_125_roundtrip)
{
    setup();
    hw_model_inject_temp(-0.125f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -0.125f, 0.001f);
}

TEST_MAIN("pct2075_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_25c_round_trip);
    RUN(test_hw_read_neg25c_round_trip);
    RUN(test_hw_read_125c_max);
    RUN(test_hw_read_neg55c_min);
    RUN(test_hw_resolution_0_125);
    RUN(test_hw_init_reads_temp_reg);
    RUN(test_hw_config_write);
    RUN(test_hw_zero_roundtrip);
    RUN(test_hw_neg0_125_roundtrip);
TEST_MAIN_END()
