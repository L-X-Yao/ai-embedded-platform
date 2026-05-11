// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ccs811.h"
#include "../ccs811.h"

static ccs811_model_t model;

static const ccs811_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const ccs811_cfg_t default_cfg = {
    .addr = CCS811_DEV_ADDR,
    .mode = CCS811_MODE_1SEC,
};

static void setup(void)
{
    ccs811_model_init(&model);
}

TEST(test_init_reads_hw_id)
{
    setup();
    ccs811_t dev;
    ccs811_err_t err = ccs811_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, CCS811_OK);
}

TEST(test_init_wrong_hw_id)
{
    setup();
    model.hw.reset_values[CCS811_REG_HW_ID] = 0xFF;
    hw_model_reset(&model.hw);
    model.hw.regs[CCS811_REG_STATUS] = 0x10;
    ccs811_t dev;
    ccs811_err_t err = ccs811_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, CCS811_ERR_ID);
}

TEST(test_init_enters_app_mode)
{
    setup();
    ccs811_t dev;
    ccs811_init(&dev, &model_i2c, &default_cfg);
    ASSERT_TRUE(model.app_mode);
}

TEST(test_hw_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, CCS811_REG_HW_ID, 0x00);
    ASSERT_EQ(model.hw.regs[CCS811_REG_HW_ID], 0x81);
}

TEST(test_read_eco2_tvoc)
{
    setup();
    ccs811_t dev;
    ccs811_init(&dev, &model_i2c, &default_cfg);

    ccs811_data_t data;
    ccs811_err_t err = ccs811_read(&dev, &data);
    ASSERT_EQ(err, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 412);
    ASSERT_EQ(data.tvoc_ppb, 5);
}

TEST(test_different_air_quality)
{
    setup();
    ccs811_t dev;
    ccs811_init(&dev, &model_i2c, &default_cfg);

    ccs811_model_set_air_quality(&model, 1500, 200);
    ccs811_data_t data;
    ccs811_read(&dev, &data);
    ASSERT_EQ(data.eco2_ppm, 1500);
    ASSERT_EQ(data.tvoc_ppb, 200);
}

TEST(test_meas_mode_written)
{
    setup();
    ccs811_t dev;
    ccs811_cfg_t cfg = { .addr = CCS811_DEV_ADDR, .mode = CCS811_MODE_60SEC };
    ccs811_init(&dev, &model_i2c, &cfg);
    /* mode 3 << 4 = 0x30 */
    ASSERT_EQ(model.hw.regs[CCS811_REG_MEAS_MODE], 0x30);
}

TEST_MAIN("ccs811 hardware model")
    RUN(test_init_reads_hw_id);
    RUN(test_init_wrong_hw_id);
    RUN(test_init_enters_app_mode);
    RUN(test_hw_id_readonly);
    RUN(test_read_eco2_tvoc);
    RUN(test_different_air_quality);
    RUN(test_meas_mode_written);
TEST_MAIN_END()
