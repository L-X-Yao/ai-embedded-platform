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

/* ---------- the model, from the manual ---------- */

static void send(uint8_t cmd)
{
    (void)hw_model_write_reg(NULL, 0x70, cmd, 0x00);
}

static void ram(uint8_t addr, uint8_t val)
{
    (void)hw_model_write_reg(NULL, 0x70, addr, val);
}

static uint8_t read1(uint8_t addr)
{
    uint8_t val = 0;
    (void)hw_model_read_regs(NULL, 0x70, addr, &val, 1);
    return val;
}

/* p.11, Power-on Reset: "Dimming is set to 16/16duty", and p.34 makes that
 * {1,1,1,1} */
TEST(test_hw_power_on_state)
{
    hw_model_reset();
    ASSERT_FALSE(ht16k33_mdl.oscillator_on);
    ASSERT_FALSE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.brightness, 15);
}

/* p.11: "In the standby mode, the HT16K33 can not accept input commands nor
 * write data to the display RAM except using the system setup command." */
TEST(test_hw_standby_accepts_only_system_setup)
{
    hw_model_reset();
    send(0x81);
    send(0xE3);
    ram(0x02, 0x55);
    ASSERT_FALSE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.brightness, 15);
    ASSERT_EQ(ht16k33_mdl.display_ram[2], 0x00);
    ASSERT_EQ(ht16k33_mdl.ignored_in_standby, 3);

    send(0x21);
    send(0x81);
    send(0xE3);
    ram(0x02, 0x55);
    ASSERT_TRUE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.brightness, 3);
    ASSERT_EQ(ht16k33_mdl.display_ram[2], 0x55);
    ASSERT_EQ(ht16k33_mdl.ignored_in_standby, 3);
}

/* p.11: with S at 0, "LED Display is in the off state." */
TEST(test_hw_s_bit_0_goes_back_to_standby)
{
    hw_model_reset();
    send(0x21);
    send(0x81);
    ram(0x00, 0x11);
    send(0x20);
    ASSERT_FALSE(ht16k33_mdl.oscillator_on);
    ASSERT_FALSE(ht16k33_mdl.display_on);
    ram(0x00, 0x22);
    ASSERT_EQ(ht16k33_mdl.display_ram[0], 0x11);
}

/* p.33: system setup is 0 0 1 0 X X X S */
TEST(test_hw_system_setup_has_three_x_bits)
{
    hw_model_reset();
    send(0x2F);
    ASSERT_TRUE(ht16k33_mdl.oscillator_on);
    send(0x2E);
    ASSERT_FALSE(ht16k33_mdl.oscillator_on);
}

/* p.33: display setup is 1 0 0 0 X B1 B0 D */
TEST(test_hw_display_setup_fields)
{
    hw_model_reset();
    send(0x21);
    send(0x87);
    ASSERT_TRUE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.blink, 3);
    send(0x89);
    ASSERT_TRUE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.blink, 0);
    send(0x82);
    ASSERT_FALSE(ht16k33_mdl.display_on);
    ASSERT_EQ(ht16k33_mdl.blink, 1);
}

/* p.33, p.34: "If programmed command data is not defined, the function will not
 * be affected." */
TEST(test_hw_undefined_commands_change_nothing)
{
    static const uint8_t undefined[] = {0x10, 0x1F, 0x30, 0x3F, 0x50, 0x70,
                                        0x90, 0xB0, 0xC0, 0xD0, 0xF0, 0xFF};
    hw_model_reset();
    send(0x21);
    send(0x81);
    send(0xE7);
    for (size_t i = 0; i < sizeof(undefined); i++) {
        ram(undefined[i], 0xFF);
        ASSERT_TRUE(ht16k33_mdl.oscillator_on);
        ASSERT_TRUE(ht16k33_mdl.display_on);
        ASSERT_EQ(ht16k33_mdl.brightness, 7);
    }
    for (uint8_t i = 0; i < 16; i++) {
        ASSERT_EQ(ht16k33_mdl.display_ram[i], 0x00);
    }
}

/* p.33: "after reaching the memory location 0X0Fh, the pointer will reset to
 * 0X00h" */
TEST(test_hw_display_ram_read_wraps)
{
    uint8_t buf[4];
    hw_model_reset();
    send(0x21);
    ram(0x0E, 0xE0);
    ram(0x0F, 0xF0);
    ram(0x00, 0x01);
    ram(0x01, 0x02);
    ASSERT_TRUE(hw_model_read_regs(NULL, 0x70, 0x0E, buf, 4));
    ASSERT_EQ_HEX(buf[0], 0xE0);
    ASSERT_EQ_HEX(buf[1], 0xF0);
    ASSERT_EQ_HEX(buf[2], 0x01);
    ASSERT_EQ_HEX(buf[3], 0x02);
}

/* p.33: Key data at 0x40 to 0x45 is read only, and p.21 has the INT flag in all
 * eight bits of 0x60 */
TEST(test_hw_key_data_and_int_flag_are_readable)
{
    static const uint8_t keys[6] = {0x01, 0x00, 0x20, 0x00, 0x00, 0x03};
    uint8_t buf[6];
    hw_model_reset();
    send(0x21);
    ASSERT_EQ_HEX(read1(0x60), 0x00);

    hw_model_set_keys(keys);
    ASSERT_EQ_HEX(read1(0x60), 0xFF);
    ASSERT_TRUE(hw_model_read_regs(NULL, 0x70, 0x40, buf, 6));
    ASSERT_MEMEQ_N(buf, keys, 6);
}

/* p.21: "after all the key data has been read that clears the key data RAM and
 * the int flag bit is set to 0" */
TEST(test_hw_reading_all_key_data_clears_it)
{
    static const uint8_t keys[6] = {0x01, 0x00, 0x20, 0x00, 0x00, 0x03};
    uint8_t buf[6];
    hw_model_reset();
    send(0x21);
    hw_model_set_keys(keys);

    ASSERT_TRUE(hw_model_read_regs(NULL, 0x70, 0x40, buf, 2));
    ASSERT_EQ_HEX(read1(0x60), 0xFF);

    ASSERT_TRUE(hw_model_read_regs(NULL, 0x70, 0x40, buf, 6));
    ASSERT_EQ_HEX(read1(0x60), 0x00);
    ASSERT_EQ_HEX(read1(0x40), 0x00);
    ASSERT_EQ_HEX(read1(0x45), 0x00);
}

/* p.33: "after reaching the memory location 0X45h, the pointer will reset to
 * 0X40h" */
TEST(test_hw_key_data_read_wraps)
{
    static const uint8_t keys[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t buf[3];
    hw_model_reset();
    send(0x21);
    hw_model_set_keys(keys);
    ASSERT_TRUE(hw_model_read_regs(NULL, 0x70, 0x44, buf, 3));
    ASSERT_EQ_HEX(buf[0], 0x55);
    ASSERT_EQ_HEX(buf[1], 0x66);
    ASSERT_EQ_HEX(buf[2], 0x11);
}

/* p.24: "The key data RAM is read only. A write to address 0x40~0x45 is
 * ignored." */
TEST(test_hw_key_data_ignores_writes)
{
    hw_model_reset();
    send(0x21);
    ram(0x40, 0x5A);
    ASSERT_EQ_HEX(read1(0x40), 0x00);
}

/* p.11: "All key data and INT flags are cleared until the standby mode is
 * canceled." */
TEST(test_hw_standby_clears_keys)
{
    static const uint8_t keys[6] = {0x01, 0, 0, 0, 0, 0};
    hw_model_reset();
    send(0x21);
    hw_model_set_keys(keys);
    send(0x20);
    ASSERT_EQ_HEX(read1(0x60), 0x00);
    ASSERT_EQ_HEX(read1(0x40), 0x00);
}

/* What the driver sends reaches a chip that starts in standby */
TEST(test_hw_driver_sends_nothing_a_standby_chip_ignores)
{
    uint8_t data[16];
    memset(data, 0x3C, sizeof(data));
    hw_model_reset();
    ASSERT_EQ(ht16k33_init(&dev, &hw_i2c, &default_cfg), HT16K33_OK);
    ASSERT_EQ(ht16k33_write_raw(&dev, data, 16), HT16K33_OK);
    ASSERT_EQ(ht16k33_set_brightness(&dev, 1), HT16K33_OK);
    ASSERT_EQ(ht16k33_clear(&dev), HT16K33_OK);
    ASSERT_EQ(ht16k33_mdl.ignored_in_standby, 0);
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
    RUN(test_hw_power_on_state);
    RUN(test_hw_standby_accepts_only_system_setup);
    RUN(test_hw_s_bit_0_goes_back_to_standby);
    RUN(test_hw_system_setup_has_three_x_bits);
    RUN(test_hw_display_setup_fields);
    RUN(test_hw_undefined_commands_change_nothing);
    RUN(test_hw_display_ram_read_wraps);
    RUN(test_hw_key_data_and_int_flag_are_readable);
    RUN(test_hw_reading_all_key_data_clears_it);
    RUN(test_hw_key_data_read_wraps);
    RUN(test_hw_key_data_ignores_writes);
    RUN(test_hw_standby_clears_keys);
    RUN(test_hw_driver_sends_nothing_a_standby_chip_ignores);
TEST_MAIN_END()
