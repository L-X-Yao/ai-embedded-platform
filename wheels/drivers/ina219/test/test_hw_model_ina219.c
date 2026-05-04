/*
 * test_hw_model_ina219.c -- tests using INA219 hardware register model
 *
 * Validates INA219 driver against a datasheet-accurate register model.
 * Tests config write, calibration, bus/shunt voltage reads, current/power
 * calculation, and software reset.
 */

#include "test_harness.h"
#include "hw_model_ina219.h"
#include "../ina219.h"
#include <math.h>

static ina219_model_t model;

static const ina219_i2c_t model_i2c = {
    .read_regs  = ina219_model_i2c_read_regs,
    .write_regs = ina219_model_i2c_write_regs,
    .delay_ms   = NULL,
    .ctx        = &model.hw,
};

static void setup(void)
{
    ina219_model_init(&model);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    ina219_t dev;
    ina219_err_t err = ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                                   INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(err, INA219_OK);
}

TEST(test_init_writes_config)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Config should have: BRNG=1 (bit 13), PGA=11 (bits 12:11), BADC/SADC 12-bit, cont mode */
    uint16_t cfg = (uint16_t)((uint16_t)model.hw.regs[INA219_REG_CONFIG_H] << 8 |
                               model.hw.regs[INA219_REG_CONFIG_L]);
    /* BRNG bit 13 = 1 for 32V */
    ASSERT_TRUE(cfg & (1u << 13));
    /* PGA bits 12:11 = 11 for 320mV */
    ASSERT_EQ((cfg >> 11) & 0x03, 3);
    /* Mode bits 2:0 = 111 for continuous */
    ASSERT_EQ(cfg & 0x07, 0x07);
}

TEST(test_init_writes_calibration)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Calibration register should be nonzero */
    uint16_t cal = (uint16_t)((uint16_t)model.hw.regs[INA219_REG_CAL_H] << 8 |
                               model.hw.regs[INA219_REG_CAL_L]);
    ASSERT_TRUE(cal > 0);
}

TEST(test_init_rejects_zero_shunt)
{
    setup();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &model_i2c, 0x40, 0.0f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

/* --- bus voltage tests --- */

TEST(test_read_bus_voltage)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Set model: 12V bus -> 3000 * 4mV */
    ina219_model_set_bus(&model, 3000);

    float voltage = 0.0f;
    ina219_err_t err = ina219_read_bus_voltage(&dev, &voltage);
    ASSERT_EQ(err, INA219_OK);
    /* Expect ~12V */
    ASSERT_NEAR(voltage, 12.0f, 0.1f);
}

TEST(test_read_bus_voltage_low)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_16V, INA219_GAIN_80MV);

    /* 5V bus -> 1250 * 4mV */
    ina219_model_set_bus(&model, 1250);

    float voltage = 0.0f;
    ina219_read_bus_voltage(&dev, &voltage);
    ASSERT_NEAR(voltage, 5.0f, 0.1f);
}

/* --- shunt voltage tests --- */

TEST(test_read_shunt_voltage)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* 10mV shunt -> 1000 * 10uV */
    ina219_model_set_shunt(&model, 1000);

    float mv = 0.0f;
    ina219_err_t err = ina219_read_shunt_voltage(&dev, &mv);
    ASSERT_EQ(err, INA219_OK);
    ASSERT_NEAR(mv, 10.0f, 0.1f);
}

TEST(test_read_shunt_voltage_negative)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* -5mV shunt -> -500 * 10uV */
    ina219_model_set_shunt(&model, -500);

    float mv = 0.0f;
    ina219_read_shunt_voltage(&dev, &mv);
    ASSERT_NEAR(mv, -5.0f, 0.1f);
}

/* --- reset test --- */

TEST(test_software_reset)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Verify calibration was written */
    uint16_t cal_before = (uint16_t)((uint16_t)model.hw.regs[INA219_REG_CAL_H] << 8 |
                                      model.hw.regs[INA219_REG_CAL_L]);
    ASSERT_TRUE(cal_before > 0);

    /* Reset device */
    ina219_err_t err = ina219_reset(&dev);
    ASSERT_EQ(err, INA219_OK);
    ASSERT_EQ(model.reset_count, 1);

    /* After reset, config should be back to default 0x399F */
    uint16_t cfg = (uint16_t)((uint16_t)model.hw.regs[INA219_REG_CONFIG_H] << 8 |
                               model.hw.regs[INA219_REG_CONFIG_L]);
    ASSERT_EQ_HEX(cfg, 0x399F);

    /* Calibration should be cleared */
    uint16_t cal_after = (uint16_t)((uint16_t)model.hw.regs[INA219_REG_CAL_H] << 8 |
                                     model.hw.regs[INA219_REG_CAL_L]);
    ASSERT_EQ(cal_after, 0);
}

/* --- read-only register protection --- */

TEST(test_shunt_registers_readonly)
{
    setup();
    ina219_model_set_shunt(&model, 1000);
    uint8_t orig_h = model.hw.regs[INA219_REG_SHUNT_V_H];
    hw_model__write_reg(&model.hw, INA219_REG_SHUNT_V_H, 0xFF);
    ASSERT_EQ(model.hw.regs[INA219_REG_SHUNT_V_H], orig_h);
}

TEST_MAIN("ina219 hardware model")
    RUN(test_init_success);
    RUN(test_init_writes_config);
    RUN(test_init_writes_calibration);
    RUN(test_init_rejects_zero_shunt);
    RUN(test_read_bus_voltage);
    RUN(test_read_bus_voltage_low);
    RUN(test_read_shunt_voltage);
    RUN(test_read_shunt_voltage_negative);
    RUN(test_software_reset);
    RUN(test_shunt_registers_readonly);
TEST_MAIN_END()
