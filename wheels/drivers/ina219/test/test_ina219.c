// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ina219.h"
#include <string.h>
#include <math.h>

/* ---------- mock I2C ---------- */

static uint8_t mock_regs[16];   /* INA219 has 6 registers, 2 bytes each */
static bool    mock_i2c_fail;
static int     mock_write_count;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_i2c_fail   = false;
    mock_write_count = 0;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    /* Each INA219 register is 2 bytes; reg index * 2 into mock_regs */
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg * 2u + i;
        buf[i] = (idx < sizeof(mock_regs)) ? mock_regs[idx] : 0;
    }
    return true;
}

static bool mock_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    mock_write_count++;
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg * 2u + i;
        if (idx < sizeof(mock_regs)) mock_regs[idx] = buf[i];
    }
    return true;
}

static const ina219_i2c_t mock_i2c = {
    .read_regs  = mock_read_regs,
    .write_regs = mock_write_regs,
    .ctx        = NULL,
};

/* Helper: set a 16-bit big-endian value at register address */
static void mock_set_reg16(uint8_t reg, uint16_t val)
{
    unsigned idx = (unsigned)reg * 2u;
    if (idx + 1 < sizeof(mock_regs)) {
        mock_regs[idx]     = (uint8_t)(val >> 8);
        mock_regs[idx + 1] = (uint8_t)(val & 0xFF);
    }
}

/* Helper: read back a 16-bit big-endian value written to mock */
static uint16_t mock_get_reg16(uint8_t reg)
{
    unsigned idx = (unsigned)reg * 2u;
    return (uint16_t)((uint16_t)mock_regs[idx] << 8 | mock_regs[idx + 1]);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);
    ASSERT_EQ(dev.dev_addr, 0x40);
    ASSERT_TRUE(dev.shunt_ohms > 0.0f);
    ASSERT_TRUE(dev.current_lsb > 0.0f);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(ina219_init(NULL, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, NULL, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_bad_shunt)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.0f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, -0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_bad_range)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           (ina219_bus_range_t)2, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_bad_gain)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, (ina219_gain_t)4), INA219_ERR_PARAM);
}

TEST(test_init_missing_write_regs)
{
    ina219_i2c_t incomplete = {
        .read_regs  = mock_read_regs,
        .write_regs = NULL,
        .ctx        = NULL,
    };
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &incomplete, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_read_bus_voltage)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    /* Bus voltage register (0x02): value in bits [15:3], LSB = 4 mV
     * 12.0V = 12000mV / 4mV = 3000 counts => 3000 << 3 = 24000 = 0x5DC0 */
    mock_set_reg16(0x02, 3000u << 3);

    float v;
    ASSERT_EQ(ina219_read_bus_voltage(&dev, &v), INA219_OK);
    ASSERT_NEAR(v, 12.0f, 0.01f);
}

TEST(test_read_shunt_voltage)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    /* Shunt voltage register (0x01): signed 16-bit, LSB = 10 uV = 0.01 mV
     * 50.0 mV => 50.0 / 0.01 = 5000 counts */
    mock_set_reg16(0x01, 5000u);

    float mv;
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, &mv), INA219_OK);
    ASSERT_NEAR(mv, 50.0f, 0.01f);
}

TEST(test_read_shunt_voltage_negative)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    /* Negative shunt voltage: -25.0 mV => -2500 counts
     * In 16-bit two's complement: (uint16_t)(-2500) = 0xF63C */
    int16_t neg = -2500;
    mock_set_reg16(0x01, (uint16_t)neg);

    float mv;
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, &mv), INA219_OK);
    ASSERT_NEAR(mv, -25.0f, 0.01f);
}

TEST(test_read_current)
{
    mock_reset();
    ina219_t dev;
    /* shunt = 0.1 ohm, gain 320mV => max_current = 0.32/0.1 = 3.2A
     * current_lsb = 3.2 / 32768 ~= 0.00009765625 A/bit */
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    /* Current register (0x04): signed 16-bit, LSB = current_lsb
     * 1000 mA = 1.0 A => 1.0 / current_lsb = 10240 counts */
    float expected_ma = 1000.0f;
    int16_t counts = (int16_t)(expected_ma / (dev.current_lsb * 1000.0f));
    mock_set_reg16(0x04, (uint16_t)counts);

    float ma;
    ASSERT_EQ(ina219_read_current(&dev, &ma), INA219_OK);
    ASSERT_NEAR(ma, expected_ma, 1.0f);
}

TEST(test_read_power)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    /* Power register (0x03): unsigned 16-bit, LSB = 20 * current_lsb (W)
     * power_lsb = 20 * current_lsb * 1000 mW
     * 500 mW => 500 / (20 * current_lsb * 1000) counts */
    float power_lsb_mw = 20.0f * dev.current_lsb * 1000.0f;
    uint16_t counts = (uint16_t)(500.0f / power_lsb_mw);
    mock_set_reg16(0x03, counts);

    float mw;
    ASSERT_EQ(ina219_read_power(&dev, &mw), INA219_OK);
    ASSERT_NEAR(mw, 500.0f, 5.0f);
}

TEST(test_config_register_range_gain)
{
    /* Verify config register bits for 16V range, gain 40mV */
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_16V, INA219_GAIN_40MV), INA219_OK);

    uint16_t config = mock_get_reg16(0x00);
    /* BRNG (bit 13) should be 0 for 16V */
    ASSERT_EQ((config >> 13) & 1, 0);
    /* PGA (bits 12:11) should be 0 for 40mV */
    ASSERT_EQ((config >> 11) & 3, 0);
    /* MODE (bits 2:0) should be 0x7 for continuous */
    ASSERT_EQ(config & 0x07, 0x07);
}

TEST(test_config_register_32v_320mv)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    uint16_t config = mock_get_reg16(0x00);
    /* BRNG (bit 13) should be 1 for 32V */
    ASSERT_EQ((config >> 13) & 1, 1);
    /* PGA (bits 12:11) should be 3 for 320mV */
    ASSERT_EQ((config >> 11) & 3, 3);
}

TEST(test_calibration_register)
{
    mock_reset();
    ina219_t dev;
    /* shunt = 0.1 ohm, gain 320mV
     * max_current = 0.32 / 0.1 = 3.2 A
     * current_lsb = 3.2 / 32768
     * cal = trunc(0.04096 / (current_lsb * 0.1)) */
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    float expected_lsb = 3.2f / 32768.0f;
    float expected_cal = 0.04096f / (expected_lsb * 0.1f);
    uint16_t cal = mock_get_reg16(0x05);
    ASSERT_EQ(cal, (uint16_t)expected_cal);
}

TEST(test_i2c_read_failure)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    mock_i2c_fail = true;
    float v;
    ASSERT_EQ(ina219_read_bus_voltage(&dev, &v), INA219_ERR_I2C);
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, &v), INA219_ERR_I2C);
    ASSERT_EQ(ina219_read_current(&dev, &v), INA219_ERR_I2C);
    ASSERT_EQ(ina219_read_power(&dev, &v), INA219_ERR_I2C);
}

TEST(test_i2c_init_failure)
{
    mock_reset();
    mock_i2c_fail = true;
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_I2C);
}

TEST(test_reset)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    ASSERT_EQ(ina219_reset(&dev), INA219_OK);
    /* Config register should have RST bit (bit 15) set */
    uint16_t config = mock_get_reg16(0x00);
    ASSERT_TRUE(config & 0x8000u);
}

TEST(test_reset_null)
{
    ASSERT_EQ(ina219_reset(NULL), INA219_ERR_PARAM);
}

TEST(test_read_null_output)
{
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_OK);

    ASSERT_EQ(ina219_read_bus_voltage(&dev, NULL), INA219_ERR_PARAM);
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, NULL), INA219_ERR_PARAM);
    ASSERT_EQ(ina219_read_current(&dev, NULL), INA219_ERR_PARAM);
    ASSERT_EQ(ina219_read_power(&dev, NULL), INA219_ERR_PARAM);
}

TEST(test_different_shunt_calibration)
{
    /* Use 0.05 ohm shunt with 80mV gain */
    mock_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &mock_i2c, 0x41, 0.05f,
                           INA219_RANGE_16V, INA219_GAIN_80MV), INA219_OK);

    /* max_current = 0.08 / 0.05 = 1.6 A
     * current_lsb = 1.6 / 32768
     * cal = trunc(0.04096 / (current_lsb * 0.05)) */
    float lsb = 1.6f / 32768.0f;
    float cal_f = 0.04096f / (lsb * 0.05f);
    uint16_t cal = mock_get_reg16(0x05);
    ASSERT_EQ(cal, (uint16_t)cal_f);
    ASSERT_EQ(dev.dev_addr, 0x41);
}

TEST_MAIN("ina219")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_bad_shunt);
    RUN(test_init_bad_range);
    RUN(test_init_bad_gain);
    RUN(test_init_missing_write_regs);
    RUN(test_read_bus_voltage);
    RUN(test_read_shunt_voltage);
    RUN(test_read_shunt_voltage_negative);
    RUN(test_read_current);
    RUN(test_read_power);
    RUN(test_config_register_range_gain);
    RUN(test_config_register_32v_320mv);
    RUN(test_calibration_register);
    RUN(test_i2c_read_failure);
    RUN(test_i2c_init_failure);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_read_null_output);
    RUN(test_different_shunt_calibration);
TEST_MAIN_END()
