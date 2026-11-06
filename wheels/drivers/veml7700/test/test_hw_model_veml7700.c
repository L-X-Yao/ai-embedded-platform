// SPDX-License-Identifier: MIT
/*
 * test_hw_model_veml7700.c — tests using hardware register model
 *
 * Validates VEML7700 driver against a datasheet-accurate register model.
 * The model enforces read-only ALS/White data registers, 16-bit LE
 * register protocol, config register behavior, and power on/off.
 */

#include "test_harness.h"
#include "hw_model_veml7700.h"
#include "../veml7700.h"
#include <math.h>

static veml7700_model_t model;

static const veml7700_i2c_t model_i2c = {
    .write_regs = veml7700_model_i2c_write_regs,
    .read_regs  = veml7700_model_i2c_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static void setup(void)
{
    veml7700_model_init(&model);
}

static uint16_t conf_word(void)
{
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    return (uint16_t)((uint16_t)model.hw.regs[conf_base] |
                      ((uint16_t)model.hw.regs[conf_base + 1] << 8));
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    veml7700_t dev;
    veml7700_err_t err = veml7700_init(&dev, &model_i2c,
                                        VEML7700_GAIN_1, VEML7700_IT_100MS);
    ASSERT_EQ(err, VEML7700_OK);
}

TEST(test_init_null_i2c)
{
    setup();
    veml7700_t dev;
    veml7700_err_t err = veml7700_init(&dev, NULL,
                                        VEML7700_GAIN_1, VEML7700_IT_100MS);
    ASSERT_EQ(err, VEML7700_ERR_PARAM);
}

TEST(test_init_writes_config)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);
    /* gain=0 (GAIN_1), IT=0 (IT_100MS), SD=0 (powered on) -> conf = 0x0000 */
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    uint16_t conf = (uint16_t)((uint16_t)model.hw.regs[conf_base] |
                    ((uint16_t)model.hw.regs[conf_base + 1] << 8));
    ASSERT_EQ(conf, 0x0000u);
}

TEST(test_init_with_gain2_it400)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_2, VEML7700_IT_400MS);
    /* gain=1 at bits[12:11], IT=2 at bits[9:6], SD=0 */
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    uint16_t conf = (uint16_t)((uint16_t)model.hw.regs[conf_base] |
                    ((uint16_t)model.hw.regs[conf_base + 1] << 8));
    /* gain=1 << 11 = 0x0800, IT=2 << 6 = 0x0080 */
    ASSERT_EQ(conf, 0x0880u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_als_raw)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint16_t raw = 0;
    veml7700_err_t err = veml7700_read_als(&dev, &raw);
    ASSERT_EQ(err, VEML7700_OK);
    ASSERT_EQ(raw, 1000);
}

TEST(test_read_white_raw)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint16_t raw = 0;
    veml7700_err_t err = veml7700_read_white(&dev, &raw);
    ASSERT_EQ(err, VEML7700_OK);
    ASSERT_EQ(raw, 1200);
}

TEST(test_read_lux_reasonable)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    float lux = 0.0f;
    veml7700_err_t err = veml7700_read_lux(&dev, &lux);
    ASSERT_EQ(err, VEML7700_OK);
    /* raw=1000, resolution at gain=1, IT=100ms = 0.0672 -> 67.2 lux */
    ASSERT_TRUE(lux > 67.1f && lux < 67.3f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_als_data_readonly)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint8_t als_base = VEML_BYTE_ADDR(VEML_REG_ALS_DATA);
    uint8_t orig = model.hw.regs[als_base];
    hw_model__write_reg(&model.hw, als_base, 0xFF);
    ASSERT_EQ(model.hw.regs[als_base], orig);
}

TEST(test_white_data_readonly)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint8_t w_base = VEML_BYTE_ADDR(VEML_REG_WHITE_DATA);
    uint8_t orig = model.hw.regs[w_base];
    hw_model__write_reg(&model.hw, w_base, 0xFF);
    ASSERT_EQ(model.hw.regs[w_base], orig);
}

TEST(test_shutdown_sets_sd_bit)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    veml7700_shutdown(&dev);
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    ASSERT_TRUE(model.hw.regs[conf_base] & 0x01u);
}

TEST(test_power_on_clears_sd_bit)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    veml7700_shutdown(&dev);
    veml7700_power_on(&dev);
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    ASSERT_FALSE(model.hw.regs[conf_base] & 0x01u);
}

/* p.7: "Command code 0 default value is 01 = devices is shut down" */
TEST(test_model_starts_shut_down)
{
    setup();
    ASSERT_EQ_HEX(conf_word(), 0x0001);
}

/* p.6: "VEML7700 can memorize the last ambient data before shutdown and keep this
 * data before waking up. When VEML7700 is in shutdown mode, the host can freely
 * read this data via read command directly." */
TEST(test_shutdown_keeps_the_last_data)
{
    setup();
    veml7700_t dev;
    ASSERT_EQ(veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
    ASSERT_EQ(veml7700_shutdown(&dev), VEML7700_OK);

    model.true_als = 2000;
    uint16_t raw = 0;
    ASSERT_EQ(veml7700_read_als(&dev, &raw), VEML7700_OK);
    ASSERT_EQ(raw, 1000);
}

/* p.7, Table 1: every gain and integration time the driver offers, as codes */
TEST(test_conf_codes_follow_table_1)
{
    static const struct { veml7700_gain_t gain; uint16_t code; } gains[] = {
        { VEML7700_GAIN_1,   0x0 }, { VEML7700_GAIN_2,   0x1 },
        { VEML7700_GAIN_1_8, 0x2 }, { VEML7700_GAIN_1_4, 0x3 },
    };
    static const struct { veml7700_it_t it; uint16_t code; } times[] = {
        { VEML7700_IT_25MS,  0xC }, { VEML7700_IT_50MS,  0x8 },
        { VEML7700_IT_100MS, 0x0 }, { VEML7700_IT_200MS, 0x1 },
        { VEML7700_IT_400MS, 0x2 }, { VEML7700_IT_800MS, 0x3 },
    };

    for (size_t g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
        for (size_t t = 0; t < sizeof(times) / sizeof(times[0]); t++) {
            setup();
            veml7700_t dev;
            ASSERT_EQ(veml7700_init(&dev, &model_i2c, gains[g].gain, times[t].it),
                      VEML7700_OK);
            ASSERT_EQ_HEX(conf_word(),
                          (uint16_t)((gains[g].code << 11) | (times[t].code << 6)));
            ASSERT_EQ(model.bad_conf_writes, 0);
        }
    }
}

/* The model against Table 1: reserved bits "Set 000b", "Set 0b", "Set 00b", and
 * ALS_IT has six codes */
TEST(test_model_flags_a_conf_table_1_does_not_have)
{
    setup();
    uint8_t reserved[2] = { 0x00, 0x20 };   /* bit 13 */
    uint8_t it_code[2]  = { 0x40, 0x01 };   /* ALS_IT 0101 */
    ASSERT_TRUE(veml7700_model_i2c_write_regs(&model, VEML7700_DEV_ADDR,
                                              VEML_REG_ALS_CONF, reserved, 2));
    ASSERT_EQ(model.bad_conf_writes, 1);
    ASSERT_TRUE(veml7700_model_i2c_write_regs(&model, VEML7700_DEV_ADDR,
                                              VEML_REG_ALS_CONF, it_code, 2));
    ASSERT_EQ(model.bad_conf_writes, 2);
}

/* init checks its settings, and the setters must not let through what init
 * refuses: nothing outside Table 1 reaches the device, and the settings stay */
TEST(test_setters_keep_undefined_codes_from_the_device)
{
    setup();
    veml7700_t dev;
    ASSERT_EQ(veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1_4, VEML7700_IT_200MS),
              VEML7700_OK);
    uint16_t before = conf_word();

    ASSERT_EQ(veml7700_set_integration_time(&dev, (veml7700_it_t)5), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_set_gain(&dev, (veml7700_gain_t)7), VEML7700_ERR_PARAM);
    ASSERT_EQ(model.bad_conf_writes, 0);
    ASSERT_EQ_HEX(conf_word(), before);
    ASSERT_EQ(dev.gain, VEML7700_GAIN_1_4);
    ASSERT_EQ(dev.it, VEML7700_IT_200MS);
}

TEST_MAIN("veml7700 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_i2c);
    RUN(test_init_writes_config);
    RUN(test_init_with_gain2_it400);
    RUN(test_read_als_raw);
    RUN(test_read_white_raw);
    RUN(test_read_lux_reasonable);
    RUN(test_als_data_readonly);
    RUN(test_white_data_readonly);
    RUN(test_shutdown_sets_sd_bit);
    RUN(test_power_on_clears_sd_bit);
    RUN(test_model_starts_shut_down);
    RUN(test_shutdown_keeps_the_last_data);
    RUN(test_conf_codes_follow_table_1);
    RUN(test_model_flags_a_conf_table_1_does_not_have);
    RUN(test_setters_keep_undefined_codes_from_the_device);
TEST_MAIN_END()
