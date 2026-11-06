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
    .delay_ms = bh1750_model_delay_ms,
    .ctx      = &model,
};

/* A HAL with no delay: the caller waits for the measurement itself */
static const bh1750_i2c_t model_i2c_no_delay = {
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

    /* raw=1200, H-RES mode: lux = 1200 / 1.2 = 1000 */
    bh1750_model_set_raw(&model, 1200);
    bh1750_init(&dev, &model_i2c, &cfg);

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

    /* raw=1200, H-RES2 mode: lux = 1200 / 2.4 = 500 */
    bh1750_model_set_raw(&model, 1200);
    bh1750_init(&dev, &model_i2c, &cfg);

    float lux = 0.0f;
    bh1750_read_lux(&dev, &lux);
    ASSERT_NEAR(lux, 500.0f, 1.0f);
}

TEST(test_read_lux_dark)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_model_set_raw(&model, 0);
    bh1750_init(&dev, &model_i2c, &cfg);

    float lux = -1.0f;
    bh1750_read_lux(&dev, &lux);
    ASSERT_NEAR(lux, 0.0f, 0.01f);
}

TEST(test_read_lux_bright)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    /* Max raw = 65535 -> 65535 / 1.2 = ~54612 lux */
    bh1750_model_set_raw(&model, 65535);
    bh1750_init(&dev, &model_i2c, &cfg);

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
    bh1750_model_set_raw(&model, 5000);
    bh1750_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(model.data_reg, 5000);

    bh1750_err_t err = bh1750_reset(&dev);
    ASSERT_EQ(err, BH1750_OK);
    /* Reset clears data register, not the light */
    ASSERT_EQ(model.data_reg, 0);
    ASSERT_EQ(model.raw_light, 5000);
}

/* --- waiting for a measurement (manual p.2, p.7) --- */

TEST(test_model_data_register_starts_at_zero)
{
    setup();
    ASSERT_EQ(model.data_reg, 0);
    uint8_t cmd = 0x10;
    bh1750_model_i2c_write(&model, BH1750_ADDR_LOW, &cmd, 1);
    ASSERT_EQ(model.data_reg, 0);          /* nothing until it ends */
    bh1750_model_advance(&model, 179);
    ASSERT_EQ(model.data_reg, 0);
    bh1750_model_advance(&model, 1);
    ASSERT_EQ(model.data_reg, 1200);
}

TEST(test_init_waits_for_h_res_measurement)
{
    setup();
    /* p.7 ex1: High Byte 10000011, Low Byte 10010000 gives 28067 lx */
    bh1750_model_set_raw(&model, 0x8390);
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c, &cfg), BH1750_OK);
    ASSERT_EQ(model.delay_total_ms, 180);

    float lux = 0.0f;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 28067.0f, 0.5f);
}

TEST(test_init_waits_for_h_res2_measurement)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES2 };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c, &cfg), BH1750_OK);
    ASSERT_EQ(model.delay_total_ms, 180);
}

TEST(test_init_waits_for_l_res_measurement)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_L_RES };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c, &cfg), BH1750_OK);
    ASSERT_EQ(model.delay_total_ms, 24);

    float lux = 0.0f;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 1000.0f, 0.5f);
}

TEST(test_continuous_read_does_not_wait)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c, &cfg), BH1750_OK);
    int cmds = model.cmd_count;
    uint32_t waited = model.delay_total_ms;

    float lux = 0.0f;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_EQ(model.cmd_count, cmds);
    ASSERT_EQ(model.delay_total_ms, waited);
}

TEST(test_no_delay_leaves_the_wait_to_the_caller)
{
    setup();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &model_i2c_no_delay, &cfg), BH1750_OK);

    float lux = -1.0f;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 0.0f, 0.01f);         /* read too early: still 0 */

    bh1750_model_advance(&model, 180);
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 1000.0f, 0.5f);
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
    RUN(test_model_data_register_starts_at_zero);
    RUN(test_init_waits_for_h_res_measurement);
    RUN(test_init_waits_for_h_res2_measurement);
    RUN(test_init_waits_for_l_res_measurement);
    RUN(test_continuous_read_does_not_wait);
    RUN(test_no_delay_leaves_the_wait_to_the_caller);
TEST_MAIN_END()
