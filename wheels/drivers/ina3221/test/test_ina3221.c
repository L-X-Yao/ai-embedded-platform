// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ina3221.h"
#include <string.h>
#include <math.h>

/* ---------- 16-bit register helpers ---------- */

/*
 * The INA3221 uses 16-bit registers accessed via read_regs/write_regs.
 * mock_i2c.h stores bytes linearly, so register R maps to
 * mock_regs[R*2] (MSB) and mock_regs[R*2+1] (LSB).
 * The mock_i2c_write_regs / mock_i2c_read_regs from mock_i2c.h write
 * buf[0..len-1] starting at mock_regs[reg]. For our 16-bit register
 * device, the driver writes 2 bytes starting at the register address.
 * We need a custom mapping: driver reg R -> mock_regs[R*2], mock_regs[R*2+1].
 */

static bool mock_i2c_fail_flag;
static int  mock_i2c_fail_after;   /* fail on the Nth I2C operation */
static int  mock_i2c_op_counter;

static uint8_t mock16_regs[512];

static void mock16_reset(void)
{
    memset(mock16_regs, 0, sizeof(mock16_regs));
    mock_i2c_fail_flag = false;
    mock_i2c_fail_after = -1;
    mock_i2c_op_counter = 0;
}

static bool mock16_should_fail(void)
{
    if (mock_i2c_fail_flag) return true;
    int n = mock_i2c_op_counter++;
    if (mock_i2c_fail_after >= 0 && n >= mock_i2c_fail_after) return true;
    return false;
}

static bool mock16_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                              const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock16_should_fail()) return false;
    unsigned base = (unsigned)reg * 2u;
    for (uint16_t i = 0; i < len; i++) {
        if (base + i < sizeof(mock16_regs))
            mock16_regs[base + i] = buf[i];
    }
    return true;
}

static bool mock16_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                             uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock16_should_fail()) return false;
    unsigned base = (unsigned)reg * 2u;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (base + i < sizeof(mock16_regs)) ? mock16_regs[base + i] : 0;
    }
    return true;
}

static void mock16_set_reg(uint8_t reg, uint16_t val)
{
    unsigned base = (unsigned)reg * 2u;
    mock16_regs[base]     = (uint8_t)(val >> 8);
    mock16_regs[base + 1] = (uint8_t)(val & 0xFF);
}

static const ina3221_i2c_t mock_i2c_hal = {
    .write_regs = mock16_write_regs,
    .read_regs  = mock16_read_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

static const ina3221_cfg_t default_cfg = {
    .addr = 0x40,
    .shunt_resistor_ohm = { 0.1f, 0.1f, 0.1f },
};

static void setup_valid_ids(void)
{
    mock16_set_reg(0xFE, 0x5449);  /* Manufacturer ID = "TI" */
    mock16_set_reg(0xFF, 0x3220);  /* Die ID */
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);
    ASSERT_EQ(dev.cfg.addr, 0x40);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(ina3221_init(NULL, &mock_i2c_hal, &default_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, NULL, &default_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, NULL), INA3221_ERR_PARAM);
}

TEST(test_init_missing_write_regs)
{
    ina3221_i2c_t bad_hal = {
        .write_regs = NULL,
        .read_regs  = mock16_read_regs,
        .ctx        = NULL,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &bad_hal, &default_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_missing_read_regs)
{
    ina3221_i2c_t bad_hal = {
        .write_regs = mock16_write_regs,
        .read_regs  = NULL,
        .ctx        = NULL,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &bad_hal, &default_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_zero_shunt)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_cfg_t cfg = {
        .addr = 0x40,
        .shunt_resistor_ohm = { 0.1f, 0.0f, 0.1f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &cfg), INA3221_ERR_PARAM);
}

TEST(test_init_negative_shunt)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_cfg_t cfg = {
        .addr = 0x40,
        .shunt_resistor_ohm = { 0.1f, 0.1f, -0.05f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &cfg), INA3221_ERR_PARAM);
}

TEST(test_init_zero_hal_struct)
{
    ina3221_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ina3221_t dev;
    ASSERT_TRUE(ina3221_init(&dev, &zero_hal, &default_cfg) < 0);
}

/* ---------- ID verification tests ---------- */

TEST(test_init_wrong_manufacturer_id)
{
    mock16_reset();
    mock16_set_reg(0xFE, 0x1234);  /* wrong */
    mock16_set_reg(0xFF, 0x3220);
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_ERR_ID);
}

TEST(test_init_wrong_die_id)
{
    mock16_reset();
    mock16_set_reg(0xFE, 0x5449);  /* correct */
    mock16_set_reg(0xFF, 0x0000);  /* wrong */
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_ERR_ID);
}

TEST(test_init_i2c_fail_on_mfr_read)
{
    mock16_reset();
    setup_valid_ids();
    mock_i2c_fail_after = 0;  /* fail on first I2C op (mfr ID read) */
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_ERR_I2C);
}

TEST(test_init_i2c_fail_on_die_read)
{
    mock16_reset();
    setup_valid_ids();
    mock_i2c_fail_after = 1;  /* fail on second I2C op (die ID read) */
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_ERR_I2C);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)INA3221_ERR_I2C);
    ASSERT_EQ(-2, (int)INA3221_ERR_ID);
    ASSERT_EQ(-3, (int)INA3221_ERR_PARAM);
}

/* ---------- read_channel tests ---------- */

TEST(test_read_channel_0)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    /* CH1 shunt: 1000 uV -> 1000/40 = 25 counts, shifted left 3 */
    int16_t shunt_counts = 25;
    mock16_set_reg(0x01, (uint16_t)(shunt_counts << 3));
    /* CH1 bus: 12000 mV -> 12000/8 = 1500 counts, shifted left 3 */
    int16_t bus_counts = 1500;
    mock16_set_reg(0x02, (uint16_t)(bus_counts << 3));

    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 1000.0f, 1.0f);
    ASSERT_NEAR(bus_mv, 12000.0f, 1.0f);
}

TEST(test_read_channel_1)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    /* CH2 shunt: 2000 uV -> 50 counts */
    mock16_set_reg(0x03, (uint16_t)(50 << 3));
    /* CH2 bus: 5000 mV -> 625 counts */
    mock16_set_reg(0x04, (uint16_t)(625 << 3));

    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 1, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 2000.0f, 1.0f);
    ASSERT_NEAR(bus_mv, 5000.0f, 1.0f);
}

TEST(test_read_channel_2)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    /* CH3 shunt: 400 uV -> 10 counts */
    mock16_set_reg(0x05, (uint16_t)(10 << 3));
    /* CH3 bus: 3300 mV -> 412 counts (3300/8 = 412.5 -> 412) */
    mock16_set_reg(0x06, (uint16_t)(412 << 3));

    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 2, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 400.0f, 1.0f);
    ASSERT_NEAR(bus_mv, 3296.0f, 1.0f);  /* 412 * 8 = 3296 */
}

TEST(test_read_channel_negative_shunt)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    /* Negative shunt: -1000 uV -> -25 counts, shifted left 3 */
    int16_t neg_counts = -25;
    mock16_set_reg(0x01, (uint16_t)(neg_counts << 3));
    mock16_set_reg(0x02, (uint16_t)(1500 << 3));

    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, -1000.0f, 1.0f);
}

TEST(test_read_channel_invalid)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 3, &shunt_uv, &bus_mv), INA3221_ERR_PARAM);
    ASSERT_EQ(ina3221_read_channel(&dev, 255, &shunt_uv, &bus_mv), INA3221_ERR_PARAM);
}

TEST(test_read_channel_null_shunt)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    float bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, NULL, &bus_mv), INA3221_ERR_PARAM);
}

TEST(test_read_channel_null_bus)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    float shunt_uv;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, NULL), INA3221_ERR_PARAM);
}

TEST(test_read_channel_null_dev)
{
    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(NULL, 0, &shunt_uv, &bus_mv), INA3221_ERR_PARAM);
}

TEST(test_read_channel_i2c_fail)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    mock_i2c_fail_flag = true;
    float shunt_uv, bus_mv;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, &bus_mv), INA3221_ERR_I2C);
}

/* ---------- read_all tests ---------- */

TEST(test_read_all)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_cfg_t cfg = {
        .addr = 0x40,
        .shunt_resistor_ohm = { 0.1f, 0.05f, 0.2f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &cfg), INA3221_OK);

    /* CH1: shunt=1000 uV (25 counts), bus=12000 mV (1500 counts) */
    mock16_set_reg(0x01, (uint16_t)(25 << 3));
    mock16_set_reg(0x02, (uint16_t)(1500 << 3));
    /* CH2: shunt=2000 uV (50 counts), bus=5000 mV (625 counts) */
    mock16_set_reg(0x03, (uint16_t)(50 << 3));
    mock16_set_reg(0x04, (uint16_t)(625 << 3));
    /* CH3: shunt=800 uV (20 counts), bus=3296 mV (412 counts) */
    mock16_set_reg(0x05, (uint16_t)(20 << 3));
    mock16_set_reg(0x06, (uint16_t)(412 << 3));

    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_OK);

    /* Shunt voltages */
    ASSERT_NEAR(data.shunt_uv[0], 1000.0f, 1.0f);
    ASSERT_NEAR(data.shunt_uv[1], 2000.0f, 1.0f);
    ASSERT_NEAR(data.shunt_uv[2],  800.0f, 1.0f);

    /* Bus voltages */
    ASSERT_NEAR(data.bus_mv[0], 12000.0f, 1.0f);
    ASSERT_NEAR(data.bus_mv[1],  5000.0f, 1.0f);
    ASSERT_NEAR(data.bus_mv[2],  3296.0f, 1.0f);

    /* Current: I(mA) = shunt_uv / (R_ohm * 1000) */
    /* CH1: 1000 / (0.1 * 1000) = 10 mA */
    ASSERT_NEAR(data.current_ma[0], 10.0f, 0.1f);
    /* CH2: 2000 / (0.05 * 1000) = 40 mA */
    ASSERT_NEAR(data.current_ma[1], 40.0f, 0.1f);
    /* CH3: 800 / (0.2 * 1000) = 4 mA */
    ASSERT_NEAR(data.current_ma[2], 4.0f, 0.1f);
}

TEST(test_read_all_null_dev)
{
    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(NULL, &data), INA3221_ERR_PARAM);
}

TEST(test_read_all_null_out)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);
    ASSERT_EQ(ina3221_read_all(&dev, NULL), INA3221_ERR_PARAM);
}

TEST(test_read_all_i2c_fail)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    mock_i2c_fail_flag = true;
    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_ERR_I2C);
}

/* ---------- current calculation tests ---------- */

TEST(test_current_calculation)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_cfg_t cfg = {
        .addr = 0x40,
        .shunt_resistor_ohm = { 0.01f, 0.1f, 1.0f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &cfg), INA3221_OK);

    /* All channels: 4000 uV shunt (100 counts) */
    mock16_set_reg(0x01, (uint16_t)(100 << 3));
    mock16_set_reg(0x02, (uint16_t)(1500 << 3));
    mock16_set_reg(0x03, (uint16_t)(100 << 3));
    mock16_set_reg(0x04, (uint16_t)(1500 << 3));
    mock16_set_reg(0x05, (uint16_t)(100 << 3));
    mock16_set_reg(0x06, (uint16_t)(1500 << 3));

    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_OK);

    /* CH1: 4000 / (0.01 * 1000) = 400 mA */
    ASSERT_NEAR(data.current_ma[0], 400.0f, 0.1f);
    /* CH2: 4000 / (0.1 * 1000) = 40 mA */
    ASSERT_NEAR(data.current_ma[1], 40.0f, 0.1f);
    /* CH3: 4000 / (1.0 * 1000) = 4 mA */
    ASSERT_NEAR(data.current_ma[2], 4.0f, 0.1f);
}

TEST(test_current_negative)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &default_cfg), INA3221_OK);

    /* Negative shunt: -2000 uV = -50 counts */
    int16_t neg = -50;
    mock16_set_reg(0x01, (uint16_t)(neg << 3));
    mock16_set_reg(0x02, (uint16_t)(1500 << 3));
    /* Zero out other channels */
    mock16_set_reg(0x03, 0);
    mock16_set_reg(0x04, 0);
    mock16_set_reg(0x05, 0);
    mock16_set_reg(0x06, 0);

    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_OK);

    /* CH1: -2000 / (0.1 * 1000) = -20 mA */
    ASSERT_NEAR(data.current_ma[0], -20.0f, 0.1f);
}

/* ---------- different addresses ---------- */

TEST(test_different_address)
{
    mock16_reset();
    setup_valid_ids();
    ina3221_cfg_t cfg = {
        .addr = 0x43,
        .shunt_resistor_ohm = { 0.1f, 0.1f, 0.1f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &mock_i2c_hal, &cfg), INA3221_OK);
    ASSERT_EQ(dev.cfg.addr, 0x43);
}

TEST_MAIN("ina3221")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_missing_write_regs);
    RUN(test_init_missing_read_regs);
    RUN(test_init_zero_shunt);
    RUN(test_init_negative_shunt);
    RUN(test_init_zero_hal_struct);
    RUN(test_init_wrong_manufacturer_id);
    RUN(test_init_wrong_die_id);
    RUN(test_init_i2c_fail_on_mfr_read);
    RUN(test_init_i2c_fail_on_die_read);
    RUN(test_error_codes);
    RUN(test_read_channel_0);
    RUN(test_read_channel_1);
    RUN(test_read_channel_2);
    RUN(test_read_channel_negative_shunt);
    RUN(test_read_channel_invalid);
    RUN(test_read_channel_null_shunt);
    RUN(test_read_channel_null_bus);
    RUN(test_read_channel_null_dev);
    RUN(test_read_channel_i2c_fail);
    RUN(test_read_all);
    RUN(test_read_all_null_dev);
    RUN(test_read_all_null_out);
    RUN(test_read_all_i2c_fail);
    RUN(test_current_calculation);
    RUN(test_current_negative);
    RUN(test_different_address);
TEST_MAIN_END()
