// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ina226.h"
#include "ina226.h"

static const ina226_i2c_t hw_i2c = {
    .read_regs  = hw_model_read_regs,
    .write_regs = hw_model_write_regs,
    .ctx        = NULL,
};

static ina226_t dev;

static const ina226_cfg_t hw_cfg = {
    .addr                  = 0x40,
    .shunt_resistance_mohm = 100,
    .max_current_ma        = 1000,
    .averaging             = INA226_AVG_1,
    .bus_conv_time         = INA226_CT_1100US,
    .shunt_conv_time       = INA226_CT_1100US,
};

TEST(test_hw_init_verifies_ids)
{
    hw_model_reset();
    ina226_err_t e = ina226_init(&dev, &hw_i2c, &hw_cfg);
    ASSERT_EQ(e, INA226_OK);
    ASSERT_TRUE(hw_read_count >= 2);  /* Read mfr ID and die ID */
}

TEST(test_hw_shunt_voltage_round_trip)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    /* Inject 5000 uV shunt voltage */
    hw_model_inject_shunt_uv(5000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.shunt_voltage_uv, 5000);
}

TEST(test_hw_bus_voltage_round_trip)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    /* Inject 12000 mV bus voltage */
    hw_model_inject_bus_mv(12000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.bus_voltage_mv, 12000u);
}

TEST(test_hw_config_register_written)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    uint16_t config = hw_model_get_reg16(0x00);
    /* Mode should be continuous shunt+bus (0x07) */
    ASSERT_EQ(config & 0x07, 0x07);
}

TEST(test_hw_calibration_nonzero)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    uint16_t cal = hw_model_get_reg16(0x05);
    ASSERT_TRUE(cal > 0);
}

TEST_MAIN("ina226_hw_model")
    RUN(test_hw_init_verifies_ids);
    RUN(test_hw_shunt_voltage_round_trip);
    RUN(test_hw_bus_voltage_round_trip);
    RUN(test_hw_config_register_written);
    RUN(test_hw_calibration_nonzero);
TEST_MAIN_END()
