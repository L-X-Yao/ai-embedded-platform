// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_adxl345.h"
#include "../adxl345.h"

static adxl345_model_t model;

static const adxl345_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const adxl345_cfg_t default_cfg = {
    .addr  = ADXL345_DEV_ADDR,
    .range = ADXL345_RANGE_2G,
};

static void setup(void)
{
    adxl345_model_init(&model);
}

TEST(test_init_reads_chip_id)
{
    setup();
    adxl345_t dev;
    adxl345_err_t err = adxl345_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, ADXL345_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[ADXL345_REG_DEVID] = 0xFF;
    hw_model_reset(&model.hw);
    adxl345_t dev;
    adxl345_err_t err = adxl345_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, ADXL345_ERR_ID);
}

TEST(test_init_sets_data_format)
{
    setup();
    adxl345_t dev;
    adxl345_cfg_t cfg = { .addr = ADXL345_DEV_ADDR, .range = ADXL345_RANGE_8G };
    adxl345_init(&dev, &model_i2c, &cfg);
    /* FULL_RES (0x08) | range 2 = 0x0A */
    ASSERT_EQ(model.hw.regs[ADXL345_REG_DATA_FORMAT], 0x0A);
}

TEST(test_init_enables_measurement)
{
    setup();
    adxl345_t dev;
    adxl345_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(model.hw.regs[ADXL345_REG_POWER_CTL] & 0x08, 0x08);
    ASSERT_TRUE(model.measuring);
}

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, ADXL345_REG_DEVID, 0x00);
    ASSERT_EQ(model.hw.regs[ADXL345_REG_DEVID], 0xE5);
}

TEST(test_data_registers_readonly)
{
    setup();
    uint8_t original = model.hw.regs[ADXL345_REG_DATAX0];
    hw_model__write_reg(&model.hw, ADXL345_REG_DATAX0, 0xFF);
    ASSERT_EQ(model.hw.regs[ADXL345_REG_DATAX0], original);
}

TEST(test_read_1g_z)
{
    setup();
    adxl345_t dev;
    adxl345_init(&dev, &model_i2c, &default_cfg);
    /* Default: 0g X, 0g Y, 1g Z (256 LSB) */
    adxl345_data_t data;
    adxl345_err_t err = adxl345_read(&dev, &data);
    ASSERT_EQ(err, ADXL345_OK);
    ASSERT_NEAR(data.z_g, 1.0f, 0.05f);
    ASSERT_NEAR(data.x_g, 0.0f, 0.001f);
}

TEST(test_different_accel)
{
    setup();
    adxl345_t dev;
    adxl345_init(&dev, &model_i2c, &default_cfg);

    adxl345_model_set_accel(&model, 128, -128, 0);
    adxl345_data_t data;
    adxl345_read(&dev, &data);
    ASSERT_TRUE(data.x_g > 0.0f);
    ASSERT_TRUE(data.y_g < 0.0f);
}

TEST(test_multiple_reads)
{
    setup();
    adxl345_t dev;
    adxl345_init(&dev, &model_i2c, &default_cfg);

    adxl345_data_t d1, d2;
    adxl345_read(&dev, &d1);
    adxl345_read(&dev, &d2);
    ASSERT_NEAR(d1.z_g, d2.z_g, 0.001f);
}

TEST_MAIN("adxl345 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_sets_data_format);
    RUN(test_init_enables_measurement);
    RUN(test_chip_id_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_read_1g_z);
    RUN(test_different_accel);
    RUN(test_multiple_reads);
TEST_MAIN_END()
