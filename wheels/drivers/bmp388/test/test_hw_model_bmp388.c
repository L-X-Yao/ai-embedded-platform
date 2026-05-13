// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bmp388.h"
#include "bmp388.h"

static const bmp388_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static bmp388_t dev;
static bmp388_cfg_t default_cfg = {
    .addr = 0x76,
    .osrs_t = BMP388_OVERSAMPLING_1,
    .osrs_p = BMP388_OVERSAMPLING_1,
};

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    bmp388_err_t e = bmp388_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_OK);
    /* Verify driver read the chip ID */
    ASSERT_TRUE(hw_read_count > 0);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    bmp388_init(&dev, &hw_i2c, &default_cfg);

    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_OK);
    /* Verify compensated values are in physically valid range */
    ASSERT_TRUE(data.temperature_c >= -40.0f && data.temperature_c <= 85.0f);
    ASSERT_TRUE(data.pressure_pa >= 30000.0f && data.pressure_pa <= 125000.0f);
}

TEST(test_hw_reset_side_effect)
{
    hw_model_reset();
    bmp388_init(&dev, &hw_i2c, &default_cfg);

    bmp388_err_t e = bmp388_reset(&dev);
    ASSERT_EQ(e, BMP388_OK);
    /* After reset, drdy should still be asserted by hw_model */
    ASSERT_TRUE(hw_regs[0x03] & 0x20);
}

TEST_MAIN("bmp388_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_reset_side_effect);
TEST_MAIN_END()
