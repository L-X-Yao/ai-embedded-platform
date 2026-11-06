// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ltr390.h"
#include "ltr390.h"

static const ltr390_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static ltr390_t dev;

static const ltr390_cfg_t default_cfg = {
    .mode       = LTR390_MODE_ALS,
    .gain       = LTR390_GAIN_3,
    .resolution = LTR390_RES_18BIT,
};

static ltr390_err_t setup(void)
{
    hw_model_reset();
    return ltr390_init(&dev, &hw_i2c, &default_cfg);
}

/* p.13: "UVS Mode: UVS+UVS Comp channels activated, ALS and Comp channels
 * deactivated", so UVS data needs UVS mode */
static void setup_uvs(void)
{
    (void)setup();
    (void)ltr390_set_mode(&dev, LTR390_MODE_UVS);
}

static void poke(uint8_t reg, uint8_t val)
{
    (void)hw_model_write_reg(NULL, LTR390_DEV_ADDR, reg, val);
}

static uint8_t peek(uint8_t reg)
{
    uint8_t val = 0xA5;
    (void)hw_model_read_regs(NULL, LTR390_DEV_ADDR, reg, &val, 1);
    return val;
}

/* ---- init ---- */

TEST(test_hw_init_ok)
{
    ltr390_err_t e = setup();
    ASSERT_EQ(e, LTR390_OK);
}

TEST(test_hw_init_reads_part_id)
{
    hw_model_reset();
    ltr390_t d;
    (void)ltr390_init(&d, &hw_i2c, &default_cfg);
    /* init must have performed at least one I2C read (part ID) */
    ASSERT_TRUE(ltr390_mdl.read_count >= 1);
}

TEST(test_hw_init_sets_gain)
{
    setup();
    /* Gain register (0x05) should be set to cfg->gain = 1 (GAIN_3) */
    ASSERT_EQ(ltr390_mdl.regs[0x05], 1);
}

TEST(test_hw_init_sets_resolution)
{
    setup();
    /* LTR-390UV-01 p.27 pseudo code: "Command = 0x22 // Resolution=18bits,
     * Meas Rate = 100ms", which is also the register default on p.14 */
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x04], 0x22u);
}

/* p.14: resolution in bits 6:4, measurement rate in bits 2:0, 010 = 100 ms.
 * Every resolution keeps the default rate. */
TEST(test_hw_init_keeps_default_rate_at_all_resolutions)
{
    static const struct { ltr390_res_t res; uint8_t reg; } rows[] = {
        { LTR390_RES_20BIT, 0x02u },  /* 000 */
        { LTR390_RES_19BIT, 0x12u },  /* 001 */
        { LTR390_RES_18BIT, 0x22u },  /* 010 */
        { LTR390_RES_17BIT, 0x32u },  /* 011 */
        { LTR390_RES_16BIT, 0x42u },  /* 100 */
        { LTR390_RES_13BIT, 0x52u },  /* 101 */
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        hw_model_reset();
        ltr390_cfg_t cfg = { .mode = LTR390_MODE_ALS, .gain = LTR390_GAIN_3,
                             .resolution = rows[i].res };
        ASSERT_EQ(ltr390_init(&dev, &hw_i2c, &cfg), LTR390_OK);
        ASSERT_EQ_HEX(ltr390_mdl.regs[0x04], rows[i].reg);
    }
}

TEST(test_hw_init_enables_sensor)
{
    setup();
    /* MAIN_CTRL (0x00): enable bit [1] set, ALS mode bit [3] clear */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x02);
}

TEST(test_hw_init_uvs_mode)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_UVS, .gain = LTR390_GAIN_1,
                         .resolution = LTR390_RES_20BIT };
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] + UVS mode [3] = 0x0A */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x0A);
}

/* ---- wrong ID ---- */

TEST(test_hw_init_wrong_id)
{
    hw_model_reset();
    ltr390_mdl.regs[0x06] = 0x55;  /* wrong part ID */
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_ERR_ID);
}

TEST(test_hw_init_id_revision_ignored)
{
    /* Part number bits [7:4] must match 0xB, but revision [3:0] can vary */
    hw_model_reset();
    ltr390_mdl.regs[0x06] = 0xB5;  /* part=0xB, rev=0x5 */
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_OK);
}

/* ---- NULL safety ---- */

TEST(test_null_dev)
{
    ASSERT_EQ(ltr390_init(NULL, &hw_i2c, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_i2c)
{
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, NULL, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_cfg)
{
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_hal_fptrs)
{
    ltr390_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &bad, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_read_als)
{
    setup();
    ASSERT_EQ(ltr390_read_als(NULL, NULL), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_read_als(&dev, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_read_uvs)
{
    setup();
    ASSERT_EQ(ltr390_read_uvs(NULL, NULL), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_read_uvs(&dev, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_set_mode)
{
    ASSERT_EQ(ltr390_set_mode(NULL, LTR390_MODE_ALS), LTR390_ERR_PARAM);
}

/* ---- read ALS ---- */

TEST(test_hw_read_als_zero)
{
    setup();
    hw_model_inject_als(0);
    hw_model_finish_conversion();
    uint32_t raw = 0xFFFFFFFF;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0u);
}

TEST(test_hw_read_als_known_value)
{
    setup();
    hw_model_inject_als(12345);
    hw_model_finish_conversion();
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 12345u);
}

TEST(test_hw_read_als_max_20bit)
{
    setup();
    hw_model_inject_als(0x000FFFFF);
    hw_model_finish_conversion();
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0x000FFFFFu);
}

TEST(test_hw_read_als_byte_order)
{
    /* Inject value 0x0A0B0C: byte0=0x0C, byte1=0x0B, byte2=0x0A */
    setup();
    hw_model_inject_als(0x000A0B0Cu);
    hw_model_finish_conversion();
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ_HEX(raw, 0x000A0B0Cu);
}

/* ---- read UVS ---- */

TEST(test_hw_read_uvs_zero)
{
    setup_uvs();
    hw_model_inject_uvs(0);
    hw_model_finish_conversion();
    uint32_t raw = 0xFFFFFFFF;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0u);
}

TEST(test_hw_read_uvs_known_value)
{
    setup_uvs();
    hw_model_inject_uvs(54321);
    hw_model_finish_conversion();
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 54321u);
}

TEST(test_hw_read_uvs_max_20bit)
{
    setup_uvs();
    hw_model_inject_uvs(0x000FFFFF);
    hw_model_finish_conversion();
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0x000FFFFFu);
}

/* ---- mode switch ---- */

TEST(test_hw_set_mode_als_to_uvs)
{
    setup();  /* starts in ALS mode */
    ltr390_err_t e = ltr390_set_mode(&dev, LTR390_MODE_UVS);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] + UVS mode [3] = 0x0A */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x0A);
}

TEST(test_hw_set_mode_uvs_to_als)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_UVS, .gain = LTR390_GAIN_3,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ltr390_init(&d, &hw_i2c, &cfg);

    ltr390_err_t e = ltr390_set_mode(&d, LTR390_MODE_ALS);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] only = 0x02 */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x02);
}

TEST(test_hw_set_mode_invalid)
{
    setup();
    ltr390_err_t e = ltr390_set_mode(&dev, (ltr390_mode_t)99);
    ASSERT_EQ(e, LTR390_ERR_PARAM);
}

/* ---- I2C error paths ---- */

TEST(test_hw_init_i2c_read_fail)
{
    hw_model_reset();
    ltr390_mdl.fail_next_read = true;
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_read_als_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_read = true;
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_read_uvs_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_read = true;
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_set_mode_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_write = true;
    ltr390_err_t e = ltr390_set_mode(&dev, LTR390_MODE_UVS);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

/* ---- error code values ---- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)LTR390_OK,        0);
    ASSERT_EQ((int)LTR390_ERR_I2C,  -1);
    ASSERT_EQ((int)LTR390_ERR_ID,   -2);
    ASSERT_EQ((int)LTR390_ERR_PARAM,-3);
}

/* ---- invalid config ---- */

TEST(test_init_invalid_gain)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_ALS, .gain = (ltr390_gain_t)99,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

TEST(test_init_invalid_resolution)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_ALS, .gain = LTR390_GAIN_1,
                         .resolution = (ltr390_res_t)99 };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

TEST(test_init_invalid_mode)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = (ltr390_mode_t)99, .gain = LTR390_GAIN_1,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

/* ---- register isolation ---- */

TEST(test_hw_als_uvs_data_isolated)
{
    setup();
    hw_model_inject_als(0x00012345u);
    hw_model_inject_uvs(0x000ABCDEu);
    hw_model_finish_conversion();
    ASSERT_EQ(ltr390_set_mode(&dev, LTR390_MODE_UVS), LTR390_OK);
    hw_model_finish_conversion();

    uint32_t als = 0, uvs = 0;
    ASSERT_EQ(ltr390_read_als(&dev, &als), LTR390_OK);
    ASSERT_EQ(ltr390_read_uvs(&dev, &uvs), LTR390_OK);
    ASSERT_EQ_HEX(als, 0x00012345u);
    ASSERT_EQ_HEX(uvs, 0x000ABCDEu);
}

/* ---- zero-init safety ---- */

TEST(test_zero_init_safety)
{
    ltr390_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ltr390_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ltr390_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

/* ---- the model against the manual ---- */

/* p.12: the reset values */
TEST(test_hw_model_reset_values)
{
    hw_model_reset();
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x00], 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x04], 0x22);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x05], 0x01);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x06], 0xB2);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x07], 0x20);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x19], 0x10);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x1A], 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x21], 0xFF);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x22], 0xFF);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x23], 0x0F);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x24], 0x00);
}

/* p.16, Power-On Status: "This flag is cleared after the register is read". Data
 * status: "will be cleared after read". */
TEST(test_hw_model_status_clears_on_read)
{
    hw_model_reset();
    ASSERT_EQ_HEX(peek(0x07), 0x20);
    ASSERT_EQ_HEX(peek(0x07), 0x00);

    poke(0x00, 0x02);
    hw_model_finish_conversion();
    ASSERT_EQ_HEX(peek(0x07), 0x08);
    ASSERT_EQ_HEX(peek(0x07), 0x00);
}

/* p.16: "UVS/ALS data is old data (Data has been read)" */
TEST(test_hw_model_reading_the_data_makes_it_old)
{
    hw_model_reset();
    poke(0x00, 0x02);
    hw_model_finish_conversion();
    ASSERT_TRUE(ltr390_mdl.regs[0x07] & 0x08);

    (void)peek(0x0D);
    ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
}

/* p.14: conversion time by resolution. p.5 and p.25: measurement starts after the
 * wake-up from standby, typically 5 ms. */
TEST(test_hw_model_conversion_time_follows_the_resolution)
{
    static const struct { uint8_t res; uint32_t us; } rows[] = {
        { 0, 400000 }, { 1, 200000 }, { 2, 100000 },
        { 3, 50000 },  { 4, 25000 },  { 5, 12500 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        hw_model_reset();
        (void)peek(0x07);
        poke(0x04, (uint8_t)(rows[i].res << 4));
        poke(0x00, 0x02);
        hw_model_advance_us(5000u + rows[i].us - 1u);
        ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
        hw_model_advance_us(1);
        ASSERT_TRUE(ltr390_mdl.regs[0x07] & 0x08);
    }
}

/* p.14: the rate sets the period, unless "the measurement rate is programmed to
 * be faster than possible for the programmed ADC measurement", when "the rate
 * will be lowered" */
TEST(test_hw_model_rate_sets_the_period)
{
    hw_model_reset();
    poke(0x04, 0x24);   /* 18 bit, 500 ms */
    poke(0x00, 0x02);
    hw_model_finish_conversion();
    (void)peek(0x07);
    hw_model_advance_us(500000u - 1u);
    ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
    hw_model_advance_us(1);
    ASSERT_TRUE(ltr390_mdl.regs[0x07] & 0x08);

    hw_model_reset();
    poke(0x04, 0x00);   /* 20 bit, 25 ms: too fast */
    poke(0x00, 0x02);
    hw_model_finish_conversion();
    (void)peek(0x07);
    hw_model_advance_us(400000u - 1u);
    ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
    hw_model_advance_us(1);
    ASSERT_TRUE(ltr390_mdl.regs[0x07] & 0x08);
}

/* p.13: "When writing to this register, it will cause a stop to any ongoing
 * measurements ALS/UVS and start new measurement." */
TEST(test_hw_model_main_ctrl_restarts_the_measurement)
{
    hw_model_reset();
    poke(0x00, 0x02);
    hw_model_advance_us(5000u + 99000u);
    poke(0x00, 0x02);
    hw_model_advance_us(99000u);
    ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
    hw_model_advance_us(1000u);
    ASSERT_TRUE(ltr390_mdl.regs[0x07] & 0x08);
}

/* p.13: bit 1 clear is standby, and nothing is measured */
TEST(test_hw_model_standby_measures_nothing)
{
    hw_model_reset();
    hw_model_inject_als(1000);
    hw_model_advance_us(1000000u);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x0D], 0x00);
    ASSERT_FALSE(ltr390_mdl.regs[0x07] & 0x08);
}

/* p.13: each mode runs its own channel */
TEST(test_hw_model_mode_picks_the_channel)
{
    hw_model_reset();
    hw_model_inject_als(0x11);
    hw_model_inject_uvs(0x22);
    poke(0x00, 0x0A);
    hw_model_finish_conversion();
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x10], 0x22);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x0D], 0x00);

    poke(0x00, 0x02);
    hw_model_finish_conversion();
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x0D], 0x11);
}

/* p.13: bit 4, "Software reset is triggered". p.12 gives what it comes back to. */
TEST(test_hw_model_software_reset)
{
    hw_model_reset();
    poke(0x05, 0x04);
    poke(0x04, 0x00);
    poke(0x21, 0x00);
    poke(0x00, 0x02);
    (void)peek(0x07);

    poke(0x00, 0x10);

    ASSERT_EQ_HEX(ltr390_mdl.regs[0x00], 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x04], 0x22);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x05], 0x01);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x21], 0xFF);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x07], 0x20);

    hw_model_inject_als(1000);
    hw_model_advance_us(1000000u);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x0D], 0x00);   /* back in standby */
}

/* p.12: the COMP data registers are read only, like ALS and UVS */
TEST(test_hw_model_read_only_registers)
{
    hw_model_reset();
    poke(0x13, 0x55);
    poke(0x18, 0x55);
    poke(0x07, 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x13], 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x18], 0x00);
    ASSERT_EQ_HEX(ltr390_mdl.regs[0x07], 0x20);
}

/* What the driver sees: straight after init no conversion has ended, so the data
 * is still the reset value. See Suspected in the README. */
TEST(test_hw_first_data_only_after_the_first_conversion)
{
    setup();
    hw_model_inject_als(1000);

    uint32_t raw = 0xFFFFFFFFu;
    ASSERT_EQ(ltr390_read_als(&dev, &raw), LTR390_OK);
    ASSERT_EQ(raw, 0u);

    hw_model_finish_conversion();
    ASSERT_EQ(ltr390_read_als(&dev, &raw), LTR390_OK);
    ASSERT_EQ(raw, 1000u);
}

TEST_MAIN("ltr390_hw_model")
    RUN(test_hw_init_ok);
    RUN(test_hw_init_reads_part_id);
    RUN(test_hw_init_sets_gain);
    RUN(test_hw_init_sets_resolution);
    RUN(test_hw_init_keeps_default_rate_at_all_resolutions);
    RUN(test_hw_init_enables_sensor);
    RUN(test_hw_init_uvs_mode);
    RUN(test_hw_init_wrong_id);
    RUN(test_hw_init_id_revision_ignored);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_null_cfg);
    RUN(test_null_hal_fptrs);
    RUN(test_null_read_als);
    RUN(test_null_read_uvs);
    RUN(test_null_set_mode);
    RUN(test_hw_read_als_zero);
    RUN(test_hw_read_als_known_value);
    RUN(test_hw_read_als_max_20bit);
    RUN(test_hw_read_als_byte_order);
    RUN(test_hw_read_uvs_zero);
    RUN(test_hw_read_uvs_known_value);
    RUN(test_hw_read_uvs_max_20bit);
    RUN(test_hw_set_mode_als_to_uvs);
    RUN(test_hw_set_mode_uvs_to_als);
    RUN(test_hw_set_mode_invalid);
    RUN(test_hw_init_i2c_read_fail);
    RUN(test_hw_read_als_i2c_fail);
    RUN(test_hw_read_uvs_i2c_fail);
    RUN(test_hw_set_mode_i2c_fail);
    RUN(test_error_codes);
    RUN(test_init_invalid_gain);
    RUN(test_init_invalid_resolution);
    RUN(test_init_invalid_mode);
    RUN(test_hw_als_uvs_data_isolated);
    RUN(test_zero_init_safety);
    RUN(test_hw_model_reset_values);
    RUN(test_hw_model_status_clears_on_read);
    RUN(test_hw_model_reading_the_data_makes_it_old);
    RUN(test_hw_model_conversion_time_follows_the_resolution);
    RUN(test_hw_model_rate_sets_the_period);
    RUN(test_hw_model_main_ctrl_restarts_the_measurement);
    RUN(test_hw_model_standby_measures_nothing);
    RUN(test_hw_model_mode_picks_the_channel);
    RUN(test_hw_model_software_reset);
    RUN(test_hw_model_read_only_registers);
    RUN(test_hw_first_data_only_after_the_first_conversion);
TEST_MAIN_END()
