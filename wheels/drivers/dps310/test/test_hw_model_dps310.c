// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_dps310.h"
#include "dps310.h"

static const dps310_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static dps310_t dev;
static dps310_cfg_t default_cfg = {
    .addr   = 0x77,
    .osrs_t = DPS310_OVERSAMPLING_1,
    .osrs_p = DPS310_OVERSAMPLING_1,
};

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    dps310_err_t e = dps310_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, DPS310_OK);
    /* Verify driver read the product ID */
    ASSERT_TRUE(hw_read_count > 0);
}

TEST(test_hw_read_25c_round_trip)
{
    hw_model_reset();
    hw_model_inject_temperature(25.0f);
    hw_model_inject_pressure(101000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 101000.0f, 50.0f);
}

TEST(test_hw_read_0c_round_trip)
{
    hw_model_reset();
    hw_model_inject_temperature(0.0f);
    hw_model_inject_pressure(100000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.01f);
    ASSERT_NEAR(data.pressure_pa, 100000.0f, 1.0f);
}

TEST(test_hw_read_negative_temp)
{
    hw_model_reset();
    hw_model_inject_temperature(-15.0f);
    hw_model_inject_pressure(95000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, -15.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 95000.0f, 50.0f);
}

TEST(test_hw_reset_side_effect)
{
    hw_model_reset();
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_err_t e = dps310_reset(&dev);
    ASSERT_EQ(e, DPS310_OK);
    /* After reset, TMP_RDY + PRS_RDY should still be asserted by hw_model */
    ASSERT_TRUE(hw_regs[0x08] & 0x30);
}

TEST(test_hw_wrong_id)
{
    hw_model_reset();
    hw_regs[0x0D] = 0xFF;
    dps310_err_t e = dps310_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, DPS310_ERR_ID);
}

TEST_MAIN("dps310_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_25c_round_trip);
    RUN(test_hw_read_0c_round_trip);
    RUN(test_hw_read_negative_temp);
    RUN(test_hw_reset_side_effect);
    RUN(test_hw_wrong_id);
TEST_MAIN_END()
