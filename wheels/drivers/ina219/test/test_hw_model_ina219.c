// SPDX-License-Identifier: MIT
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
#include <string.h>

/* ---------- fail-injectable mock for error-path tests ---------- */

static bool fail_mock_enabled;
static int  fail_mock_op_count;
static int  fail_mock_fail_at;  /* -1 = never, else fail on this op index */

static void fail_mock_reset(void)
{
    fail_mock_enabled  = false;
    fail_mock_op_count = 0;
    fail_mock_fail_at  = -1;
}

static bool fail_mock_should_fail(void)
{
    int n = fail_mock_op_count++;
    if (fail_mock_fail_at >= 0 && n == fail_mock_fail_at) return true;
    return false;
}

static bool fail_mock_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                  const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)buf; (void)len;
    if (fail_mock_should_fail()) return false;
    return true;
}

static bool fail_mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                 uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)len;
    if (fail_mock_should_fail()) return false;
    /* Return zeros so the driver doesn't get garbage */
    for (uint16_t i = 0; i < len; i++) buf[i] = 0;
    return true;
}

static const ina219_i2c_t fail_i2c = {
    .read_regs  = fail_mock_read_regs,
    .write_regs = fail_mock_write_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

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

/* --- current read tests --- */

TEST(test_read_current)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Set shunt to 10mV (1000 * 10uV) -> model computes current register */
    ina219_model_set_shunt(&model, 1000);

    float current_ma = 0.0f;
    ina219_err_t err = ina219_read_current(&dev, &current_ma);
    ASSERT_EQ(err, INA219_OK);
    /* With 10mV shunt and 0.1 ohm -> 100 mA expected */
    ASSERT_TRUE(current_ma > 0.0f);
}

TEST(test_read_current_null_dev)
{
    float ma;
    ASSERT_EQ(ina219_read_current(NULL, &ma), INA219_ERR_PARAM);
}

TEST(test_read_current_null_out)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_current(&dev, NULL), INA219_ERR_PARAM);
}

/* --- power read tests --- */

TEST(test_read_power)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    /* Set bus voltage 12V, shunt 10mV -> power register populated */
    ina219_model_set_bus(&model, 3000);
    ina219_model_set_shunt(&model, 1000);

    float power_mw = 0.0f;
    ina219_err_t err = ina219_read_power(&dev, &power_mw);
    ASSERT_EQ(err, INA219_OK);
    /* Power should be positive */
    ASSERT_TRUE(power_mw >= 0.0f);
}

TEST(test_read_power_null_dev)
{
    float mw;
    ASSERT_EQ(ina219_read_power(NULL, &mw), INA219_ERR_PARAM);
}

TEST(test_read_power_null_out)
{
    setup();
    ina219_t dev;
    ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_power(&dev, NULL), INA219_ERR_PARAM);
}

/* --- init parameter guard tests --- */

TEST(test_init_null_dev)
{
    ASSERT_EQ(ina219_init(NULL, &model_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, NULL, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

TEST(test_init_missing_read_regs)
{
    /* write_regs present but read_regs NULL — must reject */
    ina219_i2c_t no_read = {
        .read_regs  = NULL,
        .write_regs = fail_mock_write_regs,
        .delay_ms   = NULL,
        .ctx        = NULL,
    };
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &no_read, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

TEST(test_init_bad_shunt)
{
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &model_i2c, 0x40, 0.0f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
    ASSERT_EQ(ina219_init(&dev, &model_i2c, 0x40, -1.0f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

TEST(test_init_bad_range)
{
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                           (ina219_bus_range_t)2, INA219_GAIN_320MV),
              INA219_ERR_PARAM);
}

TEST(test_init_bad_gain)
{
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &model_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, (ina219_gain_t)4),
              INA219_ERR_PARAM);
}

/* --- reset null guard --- */

TEST(test_reset_null_dev)
{
    ASSERT_EQ(ina219_reset(NULL), INA219_ERR_PARAM);
}

/* --- I2C error injection using fail mock --- */

TEST(test_init_config_write_fail)
{
    /* First write_regs call (config register) fails */
    fail_mock_reset();
    fail_mock_fail_at = 0;
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_I2C);
}

TEST(test_init_cal_write_fail)
{
    /* Second write_regs call (calibration register) fails */
    fail_mock_reset();
    fail_mock_fail_at = 1;
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_ERR_I2C);
}

TEST(test_read_bus_voltage_i2c_fail)
{
    /* Init succeeds, then read fails */
    fail_mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_OK);

    fail_mock_reset();
    fail_mock_fail_at = 0;
    float v;
    ASSERT_EQ(ina219_read_bus_voltage(&dev, &v), INA219_ERR_I2C);
}

TEST(test_read_shunt_voltage_i2c_fail)
{
    fail_mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_OK);

    fail_mock_reset();
    fail_mock_fail_at = 0;
    float mv;
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, &mv), INA219_ERR_I2C);
}

TEST(test_read_current_i2c_fail)
{
    fail_mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_OK);

    fail_mock_reset();
    fail_mock_fail_at = 0;
    float ma;
    ASSERT_EQ(ina219_read_current(&dev, &ma), INA219_ERR_I2C);
}

TEST(test_read_power_i2c_fail)
{
    fail_mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &fail_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV),
              INA219_OK);

    fail_mock_reset();
    fail_mock_fail_at = 0;
    float mw;
    ASSERT_EQ(ina219_read_power(&dev, &mw), INA219_ERR_I2C);
}

TEST(test_read_bus_voltage_null_dev)
{
    float v;
    ASSERT_EQ(ina219_read_bus_voltage(NULL, &v), INA219_ERR_PARAM);
}

TEST(test_read_shunt_voltage_null_dev)
{
    float v;
    ASSERT_EQ(ina219_read_shunt_voltage(NULL, &v), INA219_ERR_PARAM);
}

TEST(test_calibration_stays_even)
{
    setup();
    ina219_t dev;
    /* 0.04096 / (0.8 A / 32768 * 0.1 ohm) comes to 16777 before the mask */
    ina219_init(&dev, &model_i2c, 0x40, 0.1f, INA219_RANGE_32V, INA219_GAIN_80MV);
    ASSERT_EQ(model.hw.regs[INA219_REG_CAL_L] & 0x01, 0);
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
    RUN(test_read_current);
    RUN(test_read_current_null_dev);
    RUN(test_read_current_null_out);
    RUN(test_read_power);
    RUN(test_read_power_null_dev);
    RUN(test_read_power_null_out);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_missing_read_regs);
    RUN(test_init_bad_shunt);
    RUN(test_init_bad_range);
    RUN(test_init_bad_gain);
    RUN(test_reset_null_dev);
    RUN(test_init_config_write_fail);
    RUN(test_init_cal_write_fail);
    RUN(test_read_bus_voltage_i2c_fail);
    RUN(test_read_shunt_voltage_i2c_fail);
    RUN(test_read_current_i2c_fail);
    RUN(test_read_power_i2c_fail);
    RUN(test_read_bus_voltage_null_dev);
    RUN(test_read_shunt_voltage_null_dev);
    RUN(test_calibration_stays_even);
TEST_MAIN_END()
