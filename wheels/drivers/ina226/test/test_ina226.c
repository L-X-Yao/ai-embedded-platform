// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "ina226.h"

/* ---------- Custom 16-bit register mock for INA226 ---------- */
/* INA226 uses 16-bit big-endian registers. Each register address holds
 * a 16-bit value, so we model as an array of uint16_t. */

#define MOCK_REG_COUNT 256

static uint16_t mock_regs16[MOCK_REG_COUNT];
static int      mock_fail_next;
static int      mock_op_count;
static int      mock_fail_at;
static int      mock_write_count;
static int      mock_read_count;

static void mock_reset(void)
{
    memset(mock_regs16, 0, sizeof(mock_regs16));
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static void mock_set_reg16(uint8_t reg, uint16_t val)
{
    mock_regs16[reg] = val;
}

static uint16_t mock_get_reg16(uint8_t reg)
{
    return mock_regs16[reg];
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    mock_read_count++;
    if (len >= 2) {
        uint16_t val = mock_regs16[reg];
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
    }
    return true;
}

static bool mock_write_regs(void *ctx, uint8_t dev_addr,
                            uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    mock_write_count++;
    if (len >= 2) {
        mock_regs16[reg] = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    }
    return true;
}

static const ina226_i2c_t mock_i2c = {
    .read_regs  = mock_read_regs,
    .write_regs = mock_write_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

static ina226_t dev;

static const ina226_cfg_t default_cfg = {
    .addr                  = INA226_ADDR_DEFAULT,
    .shunt_resistance_mohm = 100,
    .max_current_ma        = 1000,
    .averaging             = INA226_AVG_1,
    .bus_conv_time         = INA226_CT_1100US,
    .shunt_conv_time       = INA226_CT_1100US,
};

static void mock_reset_ina226(void)
{
    mock_reset();
    mock_set_reg16(0xFE, 0x5449);
    mock_set_reg16(0xFF, 0x2260);
    mock_set_reg16(0x00, 0x4127);
}

static void setup(void)
{
    mock_reset_ina226();
    ina226_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_ina226();
    ina226_err_t e = ina226_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, INA226_OK);
}

TEST(test_init_wrong_mfr_id)
{
    mock_reset_ina226();
    mock_set_reg16(0xFE, 0x0000);
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &default_cfg), INA226_ERR_ID);
}

TEST(test_init_wrong_die_id)
{
    mock_reset_ina226();
    mock_set_reg16(0xFF, 0x1234);
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &default_cfg), INA226_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset_ina226();
    mock_fail_at = 0;
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &default_cfg), INA226_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ina226_init(NULL, &mock_i2c, &default_cfg), INA226_ERR_PARAM);
    ASSERT_EQ(ina226_init(&dev, NULL, &default_cfg), INA226_ERR_PARAM);
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, NULL), INA226_ERR_PARAM);
    ASSERT_EQ(ina226_read(NULL, NULL), INA226_ERR_PARAM);
    ASSERT_EQ(ina226_reset(NULL), INA226_ERR_PARAM);
}

TEST(test_null_read_out)
{
    setup();
    ASSERT_EQ(ina226_read(&dev, NULL), INA226_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    ina226_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ina226_t zero_dev;
    ASSERT_TRUE(ina226_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_invalid_params)
{
    mock_reset_ina226();
    ina226_cfg_t bad_cfg = default_cfg;

    bad_cfg.shunt_resistance_mohm = 0;
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &bad_cfg), INA226_ERR_PARAM);

    bad_cfg = default_cfg;
    bad_cfg.max_current_ma = 0;
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &bad_cfg), INA226_ERR_PARAM);

    bad_cfg = default_cfg;
    bad_cfg.averaging = (ina226_avg_t)8;
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &bad_cfg), INA226_ERR_PARAM);

    bad_cfg = default_cfg;
    bad_cfg.bus_conv_time = (ina226_conv_time_t)8;
    ASSERT_EQ(ina226_init(&dev, &mock_i2c, &bad_cfg), INA226_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)INA226_ERR_I2C);
    ASSERT_EQ(-2, (int)INA226_ERR_ID);
    ASSERT_EQ(-3, (int)INA226_ERR_PARAM);
}

TEST(test_init_writes_config)
{
    mock_reset_ina226();
    ina226_cfg_t cfg = default_cfg;
    cfg.averaging       = INA226_AVG_16;
    cfg.bus_conv_time   = INA226_CT_588US;
    cfg.shunt_conv_time = INA226_CT_332US;

    ina226_init(&dev, &mock_i2c, &cfg);

    uint16_t config = mock_get_reg16(0x00);
    uint16_t expected = (2u << 9) | (3u << 6) | (2u << 3) | 7u;
    ASSERT_EQ_HEX(config, expected);
}

TEST(test_init_writes_calibration)
{
    mock_reset_ina226();
    ina226_init(&dev, &mock_i2c, &default_cfg);

    uint16_t cal = mock_get_reg16(0x05);
    ASSERT_TRUE(cal > 1600 && cal < 1700);
}

TEST(test_read_shunt_voltage)
{
    setup();
    mock_set_reg16(0x01, 1000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.shunt_voltage_uv, 2500);
}

TEST(test_read_shunt_voltage_negative)
{
    setup();
    mock_set_reg16(0x01, (uint16_t)(-1000));
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.shunt_voltage_uv, -2500);
}

TEST(test_read_bus_voltage)
{
    setup();
    mock_set_reg16(0x02, 8000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.bus_voltage_mv, 10000u);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_reset();
    mock_fail_at = 0;
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    mock_reset();
    ASSERT_EQ(ina226_reset(&dev), INA226_OK);
    uint16_t config = mock_get_reg16(0x00);
    ASSERT_TRUE(config & 0x8000u);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(ina226_reset(&dev), INA226_ERR_I2C);
}

TEST(test_read_current_and_power)
{
    setup();
    mock_set_reg16(0x04, 1000);
    mock_set_reg16(0x03, 100);
    mock_set_reg16(0x01, 0);
    mock_set_reg16(0x02, 0);

    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_TRUE(data.current_ma > 25 && data.current_ma < 35);
    ASSERT_TRUE(data.power_mw > 70 && data.power_mw < 85);
}

TEST(test_init_accepts_the_other_die_id)
{
    ina226_t d;
    mock_reset();
    mock_set_reg16(0xFE, 0x5449);
    mock_set_reg16(0xFF, 0x2261);
    ASSERT_EQ(ina226_init(&d, &mock_i2c, &default_cfg), INA226_OK);
}

TEST(test_calibration_fits_its_15_bits)
{
    ina226_t d;
    ina226_cfg_t cfg = default_cfg;
    cfg.shunt_resistance_mohm = 1;
    cfg.max_current_ma = 5000;
    mock_reset();
    mock_set_reg16(0xFE, 0x5449);
    mock_set_reg16(0xFF, 0x2260);
    ASSERT_EQ(ina226_init(&d, &mock_i2c, &cfg), INA226_OK);

    /* 1 mOhm at 5 A would need 33554, and bit 15 does not exist */
    ASSERT_EQ(mock_get_reg16(0x05), 0x7FFF);
    /* the scale follows the value written: 0.00512 / (32767 * 0.001) */
    ASSERT_NEAR(d.current_lsb, 0.00512f / (32767.0f * 0.001f), 0.0000001f);
}

TEST_MAIN("ina226")
    RUN(test_init_ok);
    RUN(test_init_wrong_mfr_id);
    RUN(test_init_wrong_die_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_null_read_out);
    RUN(test_zero_init_safety);
    RUN(test_invalid_params);
    RUN(test_error_codes);
    RUN(test_init_writes_config);
    RUN(test_init_writes_calibration);
    RUN(test_read_shunt_voltage);
    RUN(test_read_shunt_voltage_negative);
    RUN(test_read_bus_voltage);
    RUN(test_read_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_read_current_and_power);
    RUN(test_init_accepts_the_other_die_id);
    RUN(test_calibration_fits_its_15_bits);
TEST_MAIN_END()
