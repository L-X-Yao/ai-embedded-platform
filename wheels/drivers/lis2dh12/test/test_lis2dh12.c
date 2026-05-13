#include "test_harness.h"
#include "hw_model_lis2dh12.h"
#include "lis2dh12.h"
#include <string.h>

static const lis2dh12_i2c_t mock_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static const lis2dh12_cfg_t default_cfg = {
    .addr  = LIS2DH12_ADDR_HIGH,
    .range = LIS2DH12_RANGE_2G,
    .odr   = LIS2DH12_ODR_100HZ,
};

static lis2dh12_t dev;

TEST(test_init_ok)
{
    hw_model_reset();
    ASSERT_EQ(lis2dh12_init(&dev, &mock_i2c, &default_cfg), LIS2DH12_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(lis2dh12_init(NULL, &mock_i2c, &default_cfg), LIS2DH12_ERR_PARAM);
    ASSERT_EQ(lis2dh12_init(&dev, NULL, &default_cfg), LIS2DH12_ERR_PARAM);
    ASSERT_EQ(lis2dh12_init(&dev, &mock_i2c, NULL), LIS2DH12_ERR_PARAM);
}

TEST(test_init_zero_hal)
{
    lis2dh12_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ASSERT_TRUE(lis2dh12_init(&dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_init_wrong_id)
{
    hw_model_reset();
    hw_regs[0x0F] = 0x00;
    ASSERT_EQ(lis2dh12_init(&dev, &mock_i2c, &default_cfg), LIS2DH12_ERR_ID);
}

TEST(test_init_invalid_range)
{
    lis2dh12_cfg_t cfg = default_cfg;
    cfg.range = (lis2dh12_range_t)4;
    ASSERT_EQ(lis2dh12_init(&dev, &mock_i2c, &cfg), LIS2DH12_ERR_PARAM);
}

TEST(test_init_invalid_odr)
{
    lis2dh12_cfg_t cfg = default_cfg;
    cfg.odr = (lis2dh12_odr_t)8;
    ASSERT_EQ(lis2dh12_init(&dev, &mock_i2c, &cfg), LIS2DH12_ERR_PARAM);
}

TEST(test_init_writes_config)
{
    hw_model_reset();
    lis2dh12_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(hw_regs[0x20] & 0x07, 0x07);
    ASSERT_EQ((hw_regs[0x20] >> 4) & 0x0F, 5);
    ASSERT_TRUE(hw_regs[0x23] & 0x80);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)LIS2DH12_OK, 0);
    ASSERT_EQ((int)LIS2DH12_ERR_I2C, -1);
    ASSERT_EQ((int)LIS2DH12_ERR_ID, -2);
    ASSERT_EQ((int)LIS2DH12_ERR_PARAM, -3);
}

TEST(test_read_zero)
{
    hw_model_reset();
    lis2dh12_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_accel(0.0f, 0.0f, 0.0f, 4.0f);
    lis2dh12_data_t data;
    ASSERT_EQ(lis2dh12_read(&dev, &data), LIS2DH12_OK);
    ASSERT_NEAR(data.x_mg, 0.0f, 4.1f);
    ASSERT_NEAR(data.y_mg, 0.0f, 4.1f);
    ASSERT_NEAR(data.z_mg, 0.0f, 4.1f);
}

TEST(test_read_1g_z)
{
    hw_model_reset();
    lis2dh12_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_accel(0.0f, 0.0f, 1000.0f, 4.0f);
    lis2dh12_data_t data;
    ASSERT_EQ(lis2dh12_read(&dev, &data), LIS2DH12_OK);
    ASSERT_NEAR(data.z_mg, 1000.0f, 8.0f);
}

TEST(test_read_negative)
{
    hw_model_reset();
    lis2dh12_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_accel(-500.0f, 0.0f, 0.0f, 4.0f);
    lis2dh12_data_t data;
    ASSERT_EQ(lis2dh12_read(&dev, &data), LIS2DH12_OK);
    ASSERT_NEAR(data.x_mg, -500.0f, 8.0f);
}

TEST(test_read_4g_range)
{
    hw_model_reset();
    lis2dh12_cfg_t cfg = { .addr = LIS2DH12_ADDR_HIGH, .range = LIS2DH12_RANGE_4G, .odr = LIS2DH12_ODR_100HZ };
    lis2dh12_init(&dev, &mock_i2c, &cfg);
    hw_model_inject_accel(2000.0f, 0.0f, 0.0f, 8.0f);
    lis2dh12_data_t data;
    lis2dh12_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, 2000.0f, 16.0f);
}

TEST(test_read_null)
{
    ASSERT_EQ(lis2dh12_read(NULL, &(lis2dh12_data_t){0}), LIS2DH12_ERR_PARAM);
    hw_model_reset();
    lis2dh12_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(lis2dh12_read(&dev, NULL), LIS2DH12_ERR_PARAM);
}

TEST_MAIN("lis2dh12")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_zero_hal);
    RUN(test_init_wrong_id);
    RUN(test_init_invalid_range);
    RUN(test_init_invalid_odr);
    RUN(test_init_writes_config);
    RUN(test_error_codes);
    RUN(test_read_zero);
    RUN(test_read_1g_z);
    RUN(test_read_negative);
    RUN(test_read_4g_range);
    RUN(test_read_null);
TEST_MAIN_END()
