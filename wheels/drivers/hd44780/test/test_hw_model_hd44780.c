// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_hd44780.h"
#include "hd44780.h"

static const hd44780_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static hd44780_t dev;

TEST(test_hw_init_sends_commands)
{
    hw_model_reset();
    hd44780_err_t e = hd44780_init(&dev, &hw_i2c, 0x27, 16, 2);
    ASSERT_EQ(e, HD44780_OK);
    ASSERT_TRUE(hw_write_count > 20);
    ASSERT_TRUE(hw_delay_count > 0);
}

TEST(test_hw_backlight_bit_present_in_init)
{
    hw_model_reset();
    hd44780_init(&dev, &hw_i2c, 0x27, 16, 2);
    int bl_found = 0;
    for (int i = 0; i < hw_write_count; i++) {
        if (hw_write_log[i] & 0x08) bl_found++;
    }
    ASSERT_TRUE(bl_found > 0);
}

TEST(test_hw_write_char_has_rs_set)
{
    hw_model_reset();
    hd44780_init(&dev, &hw_i2c, 0x27, 16, 2);
    hw_model_reset();

    hd44780_write_char(&dev, 'Z');
    int rs_count = 0;
    for (int i = 0; i < hw_write_count; i++) {
        if (hw_write_log[i] & 0x01) rs_count++;
    }
    ASSERT_TRUE(rs_count > 0);
}

TEST(test_hw_clear_uses_delays)
{
    hw_model_reset();
    hd44780_init(&dev, &hw_i2c, 0x27, 16, 2);
    hw_model_reset();

    hd44780_clear(&dev);
    ASSERT_TRUE(hw_delay_count > 0);
}

TEST_MAIN("hd44780_hw_model")
    RUN(test_hw_init_sends_commands);
    RUN(test_hw_backlight_bit_present_in_init);
    RUN(test_hw_write_char_has_rs_set);
    RUN(test_hw_clear_uses_delays);
TEST_MAIN_END()
