// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_tmp117.h"
#include "tmp117.h"
#include <math.h>

static const tmp117_i2c_t hw_i2c = {
    .write_regs = hw_model_write_regs,
    .read_regs  = hw_model_read_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

static const tmp117_cfg_t cfg = {
    .addr = TMP117_ADDR_GND,
    .avg  = TMP117_AVG_NONE,
};

static tmp117_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &cfg), TMP117_OK);
}

TEST(test_hw_device_id_check)
{
    hw_model_reset();
    hw_regs16[0x0F] = 0xBEEF;
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &cfg), TMP117_ERR_ID);
}

TEST(test_hw_config_written)
{
    hw_model_reset();
    tmp117_cfg_t avg_cfg = { .addr = TMP117_ADDR_GND, .avg = TMP117_AVG_32 };
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &avg_cfg), TMP117_OK);
    ASSERT_EQ((hw_regs16[0x01] >> 5) & 0x03, 2);
}

TEST(test_hw_read_roundtrip_25c)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(25.0f);
    float temp = 0.0f;
    tmp117_read(&dev, &temp);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_hw_read_roundtrip_neg40)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(-40.0f);
    float temp = 0.0f;
    tmp117_read(&dev, &temp);
    ASSERT_NEAR(temp, -40.0f, 0.01f);
}

TEST(test_hw_reset_bit)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    tmp117_reset(&dev);
    ASSERT_TRUE(hw_regs16[0x01] & 0x0002);
}

TEST_MAIN("tmp117_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_device_id_check);
    RUN(test_hw_config_written);
    RUN(test_hw_read_roundtrip_25c);
    RUN(test_hw_read_roundtrip_neg40);
    RUN(test_hw_reset_bit);
TEST_MAIN_END()
