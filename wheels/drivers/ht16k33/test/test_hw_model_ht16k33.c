// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ht16k33.h"
#include "ht16k33.h"

static const ht16k33_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static const ht16k33_cfg_t default_cfg = {
    .addr       = 0x70,
    .brightness = 8,
};

static ht16k33_t dev;

static ht16k33_err_t setup(void)
{
    hw_model_reset();
    return ht16k33_init(&dev, &hw_i2c, &default_cfg);
}

/* ---------- init tests ---------- */

TEST(test_hw_init_sets_oscillator)
{
    ht16k33_err_t e = setup();
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_TRUE(ht16k33_mdl.oscillator_on);
}

TEST(test_hw_init_sets_display_on)
{
    setup();
    ASSERT_TRUE(ht16k33_mdl.display_on);
}

TEST(test_hw_init_sets_brightness)
{
    setup();
    ASSERT_EQ(ht16k33_mdl.brightness, 8);
}

TEST(test_hw_init_brightness_zero)
{
    hw_model_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x70, .brightness = 0 };
    ht16k33_err_t e = ht16k33_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_EQ(ht16k33_mdl.brightness, 0);
}

TEST(test_hw_init_brightness_max)
{
    hw_model_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x70, .brightness = 15 };
    ht16k33_err_t e = ht16k33_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_EQ(ht16k33_mdl.brightness, 15);
}

/* ---------- brightness tests ---------- */

TEST(test_hw_set_brightness_round_trip)
{
    setup();
    ht16k33_err_t e = ht16k33_set_brightness(&dev, 12);
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_EQ(ht16k33_mdl.brightness, 12);
}

TEST(test_hw_set_brightness_all_levels)
{
    setup();
    for (uint8_t lvl = 0; lvl <= 15; lvl++) {
        ht16k33_err_t e = ht16k33_set_brightness(&dev, lvl);
        ASSERT_EQ(e, HT16K33_OK);
        ASSERT_EQ(ht16k33_mdl.brightness, lvl);
    }
}

/* ---------- display RAM tests ---------- */

TEST(test_hw_write_raw_round_trip)
{
    setup();
    uint8_t data[16];
    for (int i = 0; i < 16; i++) {
        data[i] = (uint8_t)(0xA0 + i);
    }
    ht16k33_err_t e = ht16k33_write_raw(&dev, data, 16);
    ASSERT_EQ(e, HT16K33_OK);

    /* Verify display RAM contents in the model */
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(ht16k33_mdl.display_ram[i], (uint8_t)(0xA0 + i));
    }
}

TEST(test_hw_write_partial)
{
    setup();
    /* Write 4 bytes — only first 4 RAM bytes affected */
    uint8_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
    ht16k33_err_t e = ht16k33_write_raw(&dev, data, 4);
    ASSERT_EQ(e, HT16K33_OK);

    ASSERT_EQ(ht16k33_mdl.display_ram[0], 0x11);
    ASSERT_EQ(ht16k33_mdl.display_ram[1], 0x22);
    ASSERT_EQ(ht16k33_mdl.display_ram[2], 0x33);
    ASSERT_EQ(ht16k33_mdl.display_ram[3], 0x44);
    /* Remaining RAM still zero from init */
    ASSERT_EQ(ht16k33_mdl.display_ram[4], 0x00);
}

TEST(test_hw_clear_zeroes_ram)
{
    setup();
    /* Fill display RAM with pattern */
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    (void)ht16k33_write_raw(&dev, data, 16);

    /* Clear */
    ht16k33_err_t e = ht16k33_clear(&dev);
    ASSERT_EQ(e, HT16K33_OK);

    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(ht16k33_mdl.display_ram[i], 0x00);
    }
}

TEST(test_hw_clear_does_not_touch_settings)
{
    setup();
    /* After clear, oscillator and display should remain on */
    (void)ht16k33_clear(&dev);
    ASSERT_TRUE(ht16k33_mdl.oscillator_on);
    ASSERT_TRUE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.brightness, 8);
}

TEST(test_hw_write_count)
{
    hw_model_reset();
    ht16k33_t d;
    (void)ht16k33_init(&d, &hw_i2c, &default_cfg);
    /* Init sends 3 commands: oscillator, display, brightness */
    ASSERT_EQ(ht16k33_mdl.write_count, 3);
}

TEST_MAIN("ht16k33_hw_model")
    RUN(test_hw_init_sets_oscillator);
    RUN(test_hw_init_sets_display_on);
    RUN(test_hw_init_sets_brightness);
    RUN(test_hw_init_brightness_zero);
    RUN(test_hw_init_brightness_max);
    RUN(test_hw_set_brightness_round_trip);
    RUN(test_hw_set_brightness_all_levels);
    RUN(test_hw_write_raw_round_trip);
    RUN(test_hw_write_partial);
    RUN(test_hw_clear_zeroes_ram);
    RUN(test_hw_clear_does_not_touch_settings);
    RUN(test_hw_write_count);
TEST_MAIN_END()
