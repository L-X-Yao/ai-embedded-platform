// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_lis2dh12.h"
#include "lis2dh12.h"

static const lis2dh12_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static const lis2dh12_cfg_t cfg = {
    .addr  = LIS2DH12_ADDR_HIGH,
    .range = LIS2DH12_RANGE_2G,
    .odr   = LIS2DH12_ODR_100HZ,
};

static lis2dh12_t dev;

TEST(test_hw_init_por)
{
    hw_model_reset();
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_OK);
}

TEST(test_hw_wrong_id)
{
    hw_model_reset();
    hw_regs[0x0F] = 0x00;
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_ERR_ID);
}

TEST(test_hw_roundtrip_1g)
{
    hw_model_reset();
    lis2dh12_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_accel(0.0f, 0.0f, 1000.0f, 4.0f);
    lis2dh12_data_t data;
    lis2dh12_read(&dev, &data);
    ASSERT_NEAR(data.z_mg, 1000.0f, 8.0f);
}

TEST(test_hw_ctrl_regs_written)
{
    hw_model_reset();
    lis2dh12_init(&dev, &hw_i2c, &cfg);
    ASSERT_TRUE(hw_regs[0x20] != 0);
    ASSERT_TRUE(hw_regs[0x23] & 0x80);
}

TEST_MAIN("lis2dh12_hw_model")
    RUN(test_hw_init_por);
    RUN(test_hw_wrong_id);
    RUN(test_hw_roundtrip_1g);
    RUN(test_hw_ctrl_regs_written);
TEST_MAIN_END()
