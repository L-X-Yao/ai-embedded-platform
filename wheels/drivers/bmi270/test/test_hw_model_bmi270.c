// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bmi270.h"
#include "bmi270.h"

static const bmi270_spi_t hw_spi = {
    .cs_low   = hw_model_cs_low,
    .cs_high  = hw_model_cs_high,
    .xfer     = hw_model_xfer,
    .delay_ms = hw_model_delay_ms,
    .ctx      = NULL,
};

static bmi270_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &hw_spi, &cfg), BMI270_OK);
}

TEST(test_hw_read_accel_roundtrip)
{
    hw_model_reset();
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.acc_range = BMI270_ACC_RANGE_2G;  /* 16384 LSB/g */
    bmi270_init(&dev, &hw_spi, &cfg);

    /* Inject 1g on X axis */
    hw_model_inject_accel_gyro(16384, 0, 0, 0, 0, 0);

    bmi270_accel_gyro_t data;
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
    ASSERT_NEAR(data.ay_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.az_g, 0.0f, 0.01f);
}

TEST(test_hw_read_gyro_roundtrip)
{
    hw_model_reset();
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.gyro_range = BMI270_GYRO_RANGE_2000DPS;  /* 16.4 LSB/dps */
    bmi270_init(&dev, &hw_spi, &cfg);

    /* Inject 100 dps on Z axis */
    hw_model_inject_accel_gyro(0, 0, 0, 0, 0, 1640);

    bmi270_accel_gyro_t data;
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_OK);
    ASSERT_NEAR(data.gz_dps, 100.0f, 0.5f);
}

TEST(test_hw_read_temp)
{
    hw_model_reset();
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &hw_spi, &cfg);

    /* Inject temp = 25C: raw = (25-23)*512 = 1024 */
    hw_model_inject_temp(1024);

    float temp;
    ASSERT_EQ(bmi270_read_temp(&dev, &temp), BMI270_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_hw_range_change)
{
    hw_model_reset();
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.acc_range = BMI270_ACC_RANGE_4G;  /* 8192 LSB/g */
    bmi270_init(&dev, &hw_spi, &cfg);

    /* 1g at 4g range: raw = 8192 */
    hw_model_inject_accel_gyro(8192, 0, 0, 0, 0, 0);

    bmi270_accel_gyro_t data;
    bmi270_read_accel_gyro(&dev, &data);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);

    /* Change to 16g range */
    bmi270_set_acc_range(&dev, BMI270_ACC_RANGE_16G);

    /* Same raw now represents 8192/2048 = 4g */
    bmi270_read_accel_gyro(&dev, &data);
    ASSERT_NEAR(data.ax_g, 4.0f, 0.01f);
}

TEST_MAIN("bmi270_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_accel_roundtrip);
    RUN(test_hw_read_gyro_roundtrip);
    RUN(test_hw_read_temp);
    RUN(test_hw_range_change);
TEST_MAIN_END()
