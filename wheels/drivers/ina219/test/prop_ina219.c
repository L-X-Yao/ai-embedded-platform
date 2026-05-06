// SPDX-License-Identifier: MIT
/*
 * prop_ina219.c — property-based tests for INA219 current/voltage monitor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ina219.h"

static const ina219_i2c_t prop_i2c = {
    .read_regs  = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                (ina219_bus_range_t)(ARBI_UINT8(seed) & 1),
                (ina219_gain_t)(ARBI_UINT8(seed) % 4));
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    float voltage, shunt, current, power;
    ina219_read_bus_voltage(&dev, &voltage);
    ina219_read_shunt_voltage(&dev, &shunt);
    ina219_read_current(&dev, &current);
    ina219_read_power(&dev, &power);
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

TEST(test_read_current_null_dev)
{
    float v;
    ASSERT_EQ(ina219_read_current(NULL, &v), INA219_ERR_PARAM);
}

TEST(test_read_power_null_dev)
{
    float v;
    ASSERT_EQ(ina219_read_power(NULL, &v), INA219_ERR_PARAM);
}

TEST(test_read_bus_voltage_null_out)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_bus_voltage(&dev, NULL), INA219_ERR_PARAM);
}

TEST(test_read_shunt_voltage_null_out)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, NULL), INA219_ERR_PARAM);
}

TEST(test_read_current_null_out)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_current(&dev, NULL), INA219_ERR_PARAM);
}

TEST(test_read_power_null_out)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_read_power(&dev, NULL), INA219_ERR_PARAM);
}

TEST(test_init_write_fail)
{
    mock_i2c_reset();
    /* fail on first I2C op (write config register) */
    mock_fail_next = 1;
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_I2C);
}

TEST(test_init_cal_write_fail)
{
    mock_i2c_reset();
    /* fail on the second write (calibration register): op index 1 */
    mock_fail_at = 1;
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_I2C);
}

TEST(test_read_all_i2c_fail)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    float v;
    mock_fail_next = 1;
    ASSERT_EQ(ina219_read_bus_voltage(&dev, &v), INA219_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(ina219_read_shunt_voltage(&dev, &v), INA219_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(ina219_read_current(&dev, &v), INA219_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(ina219_read_power(&dev, &v), INA219_ERR_I2C);
}

TEST(test_reset_ok)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);
    ASSERT_EQ(ina219_reset(&dev), INA219_OK);
}

TEST(test_reset_null)
{
    ASSERT_EQ(ina219_reset(NULL), INA219_ERR_PARAM);
}

TEST(test_reset_i2c_fail)
{
    mock_i2c_reset();
    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    mock_fail_next = 1;
    ASSERT_EQ(ina219_reset(&dev), INA219_ERR_I2C);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(ina219_init(NULL, &prop_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_missing_callbacks)
{
    ina219_t dev;
    ina219_i2c_t no_write = {
        .read_regs  = mock_i2c_read_regs,
        .write_regs = NULL,
        .ctx        = NULL,
    };
    ASSERT_EQ(ina219_init(&dev, &no_write, 0x40, 0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
    ina219_i2c_t no_read = {
        .read_regs  = NULL,
        .write_regs = mock_i2c_write_regs,
        .ctx        = NULL,
    };
    ASSERT_EQ(ina219_init(&dev, &no_read, 0x40, 0.1f,
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
    mock_i2c_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, 0.0f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, -0.1f,
                           INA219_RANGE_32V, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_bad_range)
{
    mock_i2c_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                           (ina219_bus_range_t)2, INA219_GAIN_320MV), INA219_ERR_PARAM);
}

TEST(test_init_bad_gain)
{
    mock_i2c_reset();
    ina219_t dev;
    ASSERT_EQ(ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                           INA219_RANGE_32V, (ina219_gain_t)4), INA219_ERR_PARAM);
}

TEST_MAIN("prop_ina219")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(test_read_bus_voltage_null_dev);
    RUN(test_read_shunt_voltage_null_dev);
    RUN(test_read_current_null_dev);
    RUN(test_read_power_null_dev);
    RUN(test_read_bus_voltage_null_out);
    RUN(test_read_shunt_voltage_null_out);
    RUN(test_read_current_null_out);
    RUN(test_read_power_null_out);
    RUN(test_init_write_fail);
    RUN(test_init_cal_write_fail);
    RUN(test_read_all_i2c_fail);
    RUN(test_reset_ok);
    RUN(test_reset_null);
    RUN(test_reset_i2c_fail);
    RUN(test_init_null_dev);
    RUN(test_init_missing_callbacks);
    RUN(test_init_null_i2c);
    RUN(test_init_bad_shunt);
    RUN(test_init_bad_range);
    RUN(test_init_bad_gain);
TEST_MAIN_END()
