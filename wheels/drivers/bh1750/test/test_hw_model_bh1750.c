// SPDX-License-Identifier: MIT
/*
 * test_hw_model_bh1750.c -- tests using BH1750 hardware model
 *
 * Validates BH1750 driver against a command-based hardware model.
 * Tests init sequence (power on + mode set), lux calculation for
 * different modes, power on/off commands, and reset behavior.
 */

#include "test_harness.h"
#include "hw_model_bh1750.h"
#include "../bh1750.h"
#include <math.h>

static bh1750_model_t model;

static const bh1750_i2c_t model_i2c = {
    .write    = bh1750_model_i2c_write,
    .read     = bh1750_model_i2c_read,
    .delay_ms = NULL,
    .ctx      = &model,
};

static void setup(void)
{
    bh1750_model_init(&model);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_err_t err = bh1750_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(err, BH1750_OK);
    /* Init sends: power on (0x01) + mode (0x10) */
    ASSERT_TRUE(model.powered);
    ASSERT_EQ(model.mode, 0x10);
    ASSERT_EQ(model.cmd_count, 2);
}

TEST(test_init_h_res2_mode)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES2 };
    bh1750_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(model.mode, 0x11);
}

TEST(test_init_rejects_invalid_mode)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = 0xFF };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c, &cfg), BH1750_ERR_PARAM);
}

/* --- lux reading tests --- */

TEST(test_read_lux_h_res)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);

    /* raw=1200, H-RES mode: lux = 1200 / 1.2 = 1000 */
    bh1750_model_set_raw(&model, 1200);

    float lux = 0.0f;
    bh1750_err_t err = bh1750_read_lux(&dev, &lux);
    ASSERT_EQ(err, BH1750_OK);
    ASSERT_NEAR(lux, 1000.0f, 1.0f);
}

TEST(test_read_lux_h_res2)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES2 };
    bh1750_init(&dev, &model_i2c, &cfg);

    /* raw=1200, H-RES2 mode: lux = 1200 / 2.4 = 500 */
    bh1750_model_set_raw(&model, 1200);

    float lux = 0.0f;
    bh1750_read_lux(&dev, &lux);
    ASSERT_NEAR(lux, 500.0f, 1.0f);
}

TEST(test_read_lux_dark)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);

    bh1750_model_set_raw(&model, 0);
    float lux = -1.0f;
    bh1750_read_lux(&dev, &lux);
    ASSERT_NEAR(lux, 0.0f, 0.01f);
}

TEST(test_read_lux_bright)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);

    /* Max raw = 65535 -> 65535 / 1.2 = ~54612 lux */
    bh1750_model_set_raw(&model, 65535);
    float lux = 0.0f;
    bh1750_read_lux(&dev, &lux);
    ASSERT_TRUE(lux > 50000.0f);
}

/* --- power control tests --- */

TEST(test_power_off)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);

    bh1750_err_t err = bh1750_power_off(&dev);
    ASSERT_EQ(err, BH1750_OK);
    ASSERT_FALSE(model.powered);
}

TEST(test_power_on)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);
    bh1750_power_off(&dev);

    bh1750_err_t err = bh1750_power_on(&dev);
    ASSERT_EQ(err, BH1750_OK);
    ASSERT_TRUE(model.powered);
}

/* --- reset test --- */

TEST(test_reset)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &model_i2c, &cfg);

    bh1750_model_set_raw(&model, 5000);
    bh1750_err_t err = bh1750_reset(&dev);
    ASSERT_EQ(err, BH1750_OK);
    /* Reset clears data register */
    ASSERT_EQ(model.raw_light, 0);
}

TEST_MAIN("bh1750 hardware model")
    RUN(test_init_success);
    RUN(test_init_h_res2_mode);
    RUN(test_init_rejects_invalid_mode);
    RUN(test_read_lux_h_res);
    RUN(test_read_lux_h_res2);
    RUN(test_read_lux_dark);
    RUN(test_read_lux_bright);
    RUN(test_power_off);
    RUN(test_power_on);
    RUN(test_reset);
TEST_MAIN_END()
