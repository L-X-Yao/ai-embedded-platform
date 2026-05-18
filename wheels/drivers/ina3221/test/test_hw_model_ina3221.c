// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ina3221.c -- tests using INA3221 hardware register model
 *
 * Validates INA3221 driver against a datasheet-accurate register model
 * backed by uint16_t[256]. Tests ID verification, shunt/bus voltage reads,
 * current calculation, multi-channel round-trips, and error injection.
 */

#include "test_harness.h"
#include "hw_model_ina3221.h"
#include "../ina3221.h"
#include <math.h>
#include <string.h>

/* ---------- fail-injectable wrapper around hw_model ---------- */

static int  fail_mock_op_count;
static int  fail_mock_fail_at;  /* -1 = never, else fail on this op index */

static void fail_mock_reset(void)
{
    fail_mock_op_count = 0;
    fail_mock_fail_at  = -1;
}

static bool fail_mock_should_fail(void)
{
    int n = fail_mock_op_count++;
    if (fail_mock_fail_at >= 0 && n == fail_mock_fail_at) return true;
    return false;
}

/*
 * These wrap the real hw_model callbacks but inject failures at
 * the transport layer, so non-failing ops return correct register data.
 */
static bool fail_mock_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                 const uint8_t *buf, uint16_t len)
{
    if (fail_mock_should_fail()) return false;
    return ina3221_model_i2c_write_regs(ctx, dev_addr, reg, buf, len);
}

static bool fail_mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                uint8_t *buf, uint16_t len)
{
    if (fail_mock_should_fail()) return false;
    return ina3221_model_i2c_read_regs(ctx, dev_addr, reg, buf, len);
}

/* ---------- hw_model setup ---------- */

static ina3221_model_t model;

static const ina3221_i2c_t model_i2c = {
    .read_regs  = ina3221_model_i2c_read_regs,
    .write_regs = ina3221_model_i2c_write_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static const ina3221_cfg_t model_cfg = {
    .addr = 0x40,
    .shunt_resistor_ohm = { 0.1f, 0.05f, 0.2f },
};

static void setup(void)
{
    ina3221_model_init(&model);
}

/* ---------- init tests ---------- */

TEST(test_init_success)
{
    setup();
    ina3221_t dev;
    ina3221_err_t err = ina3221_init(&dev, &model_i2c, &model_cfg);
    ASSERT_EQ(err, INA3221_OK);
}

TEST(test_init_verifies_manufacturer_id)
{
    setup();
    /* Corrupt manufacturer ID in model */
    model.regs[INA3221_REG_MFR_ID] = 0x0000;

    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &model_i2c, &model_cfg), INA3221_ERR_ID);
}

TEST(test_init_verifies_die_id)
{
    setup();
    /* Corrupt die ID in model */
    model.regs[INA3221_REG_DIE_ID] = 0xFFFF;

    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &model_i2c, &model_cfg), INA3221_ERR_ID);
}

TEST(test_init_rejects_zero_shunt)
{
    setup();
    ina3221_cfg_t cfg = {
        .addr = 0x40,
        .shunt_resistor_ohm = { 0.1f, 0.0f, 0.1f },
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &model_i2c, &cfg), INA3221_ERR_PARAM);
}

/* ---------- ID register POR defaults ---------- */

TEST(test_por_manufacturer_id)
{
    setup();
    ASSERT_EQ_HEX(model.regs[INA3221_REG_MFR_ID], 0x5449);
}

TEST(test_por_die_id)
{
    setup();
    ASSERT_EQ_HEX(model.regs[INA3221_REG_DIE_ID], 0x3220);
}

/* ---------- readonly protection ---------- */

TEST(test_id_registers_readonly)
{
    setup();
    /* Attempt to write via I2C -- should be silently ignored */
    uint8_t buf[2] = {0xFF, 0xFF};
    ina3221_model_i2c_write_regs(&model, 0x40, INA3221_REG_MFR_ID, buf, 2);
    ASSERT_EQ_HEX(model.regs[INA3221_REG_MFR_ID], 0x5449);

    ina3221_model_i2c_write_regs(&model, 0x40, INA3221_REG_DIE_ID, buf, 2);
    ASSERT_EQ_HEX(model.regs[INA3221_REG_DIE_ID], 0x3220);
}

TEST(test_shunt_registers_readonly)
{
    setup();
    ina3221_model_inject_shunt(&model, 0, 1000.0f);
    uint16_t orig = model.regs[INA3221_REG_CH1_SHUNT_V];

    uint8_t buf[2] = {0xFF, 0xFF};
    ina3221_model_i2c_write_regs(&model, 0x40, INA3221_REG_CH1_SHUNT_V, buf, 2);
    ASSERT_EQ_HEX(model.regs[INA3221_REG_CH1_SHUNT_V], orig);
}

TEST(test_bus_registers_readonly)
{
    setup();
    ina3221_model_inject_bus(&model, 0, 12000.0f);
    uint16_t orig = model.regs[INA3221_REG_CH1_BUS_V];

    uint8_t buf[2] = {0xFF, 0xFF};
    ina3221_model_i2c_write_regs(&model, 0x40, INA3221_REG_CH1_BUS_V, buf, 2);
    ASSERT_EQ_HEX(model.regs[INA3221_REG_CH1_BUS_V], orig);
}

/* ---------- single channel read ---------- */

TEST(test_read_channel_ch1)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    ina3221_model_inject_shunt(&model, 0, 1000.0f);  /* 1000 uV */
    ina3221_model_inject_bus(&model, 0, 12000.0f);    /* 12V */

    float shunt_uv = 0.0f, bus_mv = 0.0f;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 1000.0f, 41.0f);  /* 1 LSB = 40 uV tolerance */
    ASSERT_NEAR(bus_mv, 12000.0f, 9.0f);     /* 1 LSB = 8 mV tolerance */
}

TEST(test_read_channel_ch2)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    ina3221_model_inject_shunt(&model, 1, 5000.0f);
    ina3221_model_inject_bus(&model, 1, 3300.0f);

    float shunt_uv = 0.0f, bus_mv = 0.0f;
    ASSERT_EQ(ina3221_read_channel(&dev, 1, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 5000.0f, 41.0f);
    ASSERT_NEAR(bus_mv, 3300.0f, 9.0f);
}

TEST(test_read_channel_ch3)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    ina3221_model_inject_shunt(&model, 2, 200.0f);
    ina3221_model_inject_bus(&model, 2, 5000.0f);

    float shunt_uv = 0.0f, bus_mv = 0.0f;
    ASSERT_EQ(ina3221_read_channel(&dev, 2, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, 200.0f, 41.0f);
    ASSERT_NEAR(bus_mv, 5000.0f, 9.0f);
}

/* ---------- negative shunt ---------- */

TEST(test_read_channel_negative_shunt)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    ina3221_model_inject_shunt(&model, 0, -2000.0f);
    ina3221_model_inject_bus(&model, 0, 12000.0f);

    float shunt_uv = 0.0f, bus_mv = 0.0f;
    ASSERT_EQ(ina3221_read_channel(&dev, 0, &shunt_uv, &bus_mv), INA3221_OK);
    ASSERT_NEAR(shunt_uv, -2000.0f, 41.0f);
}

/* ---------- read all channels + current calculation ---------- */

TEST(test_read_all_channels)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    /* CH1: 1000 uV shunt, 12000 mV bus, R=0.1 ohm -> 10 mA */
    ina3221_model_inject_shunt(&model, 0, 1000.0f);
    ina3221_model_inject_bus(&model, 0, 12000.0f);

    /* CH2: 5000 uV shunt, 5000 mV bus, R=0.05 ohm -> 100 mA */
    ina3221_model_inject_shunt(&model, 1, 5000.0f);
    ina3221_model_inject_bus(&model, 1, 5000.0f);

    /* CH3: 800 uV shunt, 3300 mV bus, R=0.2 ohm -> 4 mA */
    ina3221_model_inject_shunt(&model, 2, 800.0f);
    ina3221_model_inject_bus(&model, 2, 3300.0f);

    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_OK);

    ASSERT_NEAR(data.shunt_uv[0], 1000.0f, 41.0f);
    ASSERT_NEAR(data.shunt_uv[1], 5000.0f, 41.0f);
    ASSERT_NEAR(data.shunt_uv[2],  800.0f, 41.0f);

    ASSERT_NEAR(data.bus_mv[0], 12000.0f, 9.0f);
    ASSERT_NEAR(data.bus_mv[1],  5000.0f, 9.0f);
    ASSERT_NEAR(data.bus_mv[2],  3300.0f, 9.0f);

    /* Current with quantisation tolerance propagated */
    ASSERT_NEAR(data.current_ma[0],  10.0f, 1.0f);
    ASSERT_NEAR(data.current_ma[1], 100.0f, 2.0f);
    ASSERT_NEAR(data.current_ma[2],   4.0f, 1.0f);
}

/* ---------- current with negative shunt ---------- */

TEST(test_read_all_negative_current)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);

    ina3221_model_inject_shunt(&model, 0, -2000.0f);
    ina3221_model_inject_bus(&model, 0, 12000.0f);
    ina3221_model_inject_shunt(&model, 1, 0.0f);
    ina3221_model_inject_bus(&model, 1, 0.0f);
    ina3221_model_inject_shunt(&model, 2, 0.0f);
    ina3221_model_inject_bus(&model, 2, 0.0f);

    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(&dev, &data), INA3221_OK);
    /* CH1: -2000 / (0.1 * 1000) = -20 mA */
    ASSERT_NEAR(data.current_ma[0], -20.0f, 2.0f);
}

/* ---------- parameter guard tests ---------- */

TEST(test_init_null_dev)
{
    ASSERT_EQ(ina3221_init(NULL, &model_i2c, &model_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, NULL, &model_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &model_i2c, NULL), INA3221_ERR_PARAM);
}

TEST(test_init_missing_read_regs)
{
    ina3221_i2c_t no_read = {
        .read_regs  = NULL,
        .write_regs = fail_mock_write_regs,
        .delay_ms   = NULL,
        .ctx        = NULL,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &no_read, &model_cfg), INA3221_ERR_PARAM);
}

TEST(test_init_missing_write_regs)
{
    ina3221_i2c_t no_write = {
        .read_regs  = fail_mock_read_regs,
        .write_regs = NULL,
        .delay_ms   = NULL,
        .ctx        = NULL,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &no_write, &model_cfg), INA3221_ERR_PARAM);
}

/* ---------- I2C error injection ---------- */

TEST(test_init_i2c_fail_mfr)
{
    setup();
    fail_mock_reset();
    fail_mock_fail_at = 0;  /* first read (mfr ID) fails */
    ina3221_i2c_t fail_i2c = {
        .read_regs  = fail_mock_read_regs,
        .write_regs = fail_mock_write_regs,
        .delay_ms   = NULL,
        .ctx        = &model,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &fail_i2c, &model_cfg), INA3221_ERR_I2C);
}

TEST(test_init_i2c_fail_die)
{
    setup();
    fail_mock_reset();
    fail_mock_fail_at = 1;  /* second read (die ID) fails */
    ina3221_i2c_t fail_i2c = {
        .read_regs  = fail_mock_read_regs,
        .write_regs = fail_mock_write_regs,
        .delay_ms   = NULL,
        .ctx        = &model,
    };
    ina3221_t dev;
    ASSERT_EQ(ina3221_init(&dev, &fail_i2c, &model_cfg), INA3221_ERR_I2C);
}

TEST(test_read_channel_null_dev_hw)
{
    float s, b;
    ASSERT_EQ(ina3221_read_channel(NULL, 0, &s, &b), INA3221_ERR_PARAM);
}

TEST(test_read_all_null_dev_hw)
{
    ina3221_data_t data;
    ASSERT_EQ(ina3221_read_all(NULL, &data), INA3221_ERR_PARAM);
}

TEST(test_read_all_null_out_hw)
{
    setup();
    ina3221_t dev;
    ina3221_init(&dev, &model_i2c, &model_cfg);
    ASSERT_EQ(ina3221_read_all(&dev, NULL), INA3221_ERR_PARAM);
}

TEST_MAIN("ina3221 hardware model")
    RUN(test_init_success);
    RUN(test_init_verifies_manufacturer_id);
    RUN(test_init_verifies_die_id);
    RUN(test_init_rejects_zero_shunt);
    RUN(test_por_manufacturer_id);
    RUN(test_por_die_id);
    RUN(test_id_registers_readonly);
    RUN(test_shunt_registers_readonly);
    RUN(test_bus_registers_readonly);
    RUN(test_read_channel_ch1);
    RUN(test_read_channel_ch2);
    RUN(test_read_channel_ch3);
    RUN(test_read_channel_negative_shunt);
    RUN(test_read_all_channels);
    RUN(test_read_all_negative_current);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_missing_read_regs);
    RUN(test_init_missing_write_regs);
    RUN(test_init_i2c_fail_mfr);
    RUN(test_init_i2c_fail_die);
    RUN(test_read_channel_null_dev_hw);
    RUN(test_read_all_null_dev_hw);
    RUN(test_read_all_null_out_hw);
TEST_MAIN_END()
