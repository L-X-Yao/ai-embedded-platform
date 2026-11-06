// SPDX-License-Identifier: MIT
/*
 * test_hw_model_max7219.c — tests using MAX7219 hardware register model
 *
 * Validates MAX7219 driver against a datasheet-accurate register model
 * with SPI-to-register shim. Tests init sequence, digit writes,
 * intensity masking, scan limit, shutdown, decode mode, and display test.
 */

#include "test_harness.h"
#include "hw_model_max7219.h"
#include "../max7219.h"

static max7219_model_t model;

static max7219_spi_t model_spi = {
    .transfer = max7219_model_spi_transfer,
    .cs_low   = max7219_model_cs_low,
    .cs_high  = max7219_model_cs_high,
    .ctx      = &model,
};

static void setup(void)
{
    max7219_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_exits_shutdown)
{
    setup();
    max7219_t dev;
    max7219_err_t err = max7219_init(&dev, &model_spi);
    ASSERT_EQ(err, MAX7219_OK);
    /* After init, SHUTDOWN register should be 0x01 (normal operation) */
    ASSERT_EQ(model.hw.regs[MAX7219_REG_SHUTDOWN], 0x01);
    ASSERT_FALSE(model.shutdown);
}

TEST(test_init_disables_display_test)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DISPLAY_TEST], 0x00);
    ASSERT_FALSE(model.display_test);
}

TEST(test_init_sets_scan_limit_all)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);
    /* Init sets scan limit to 7 (all 8 digits) */
    ASSERT_EQ(model.hw.regs[MAX7219_REG_SCAN_LIMIT], 0x07);
}

TEST(test_init_sets_no_decode)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);
    /* Init sets decode mode to 0x00 (no BCD decode) */
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DECODE_MODE], 0x00);
}

TEST(test_init_sets_default_intensity)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);
    /* Init sets intensity to 0x08 (mid-range) */
    ASSERT_EQ(model.hw.regs[MAX7219_REG_INTENSITY], 0x08);
}

TEST(test_init_clears_all_digits)
{
    setup();
    /* Pre-fill digits */
    for (int i = 0; i < 8; i++) {
        model.hw.regs[MAX7219_REG_DIGIT0 + i] = 0xFF;
    }
    max7219_t dev;
    max7219_init(&dev, &model_spi);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(model.hw.regs[MAX7219_REG_DIGIT0 + i], 0x00);
    }
}

/* --- digit writes ------------------------------------------------------ */

TEST(test_set_digit_writes_register)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    max7219_err_t err = max7219_set_digit(&dev, 0, 0x7E);
    ASSERT_EQ(err, MAX7219_OK);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DIGIT0], 0x7E);

    err = max7219_set_digit(&dev, 7, 0xAB);
    ASSERT_EQ(err, MAX7219_OK);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DIGIT7], 0xAB);
}

/* --- intensity masking ------------------------------------------------- */

TEST(test_intensity_register_masked)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    /* Write intensity value 0x0C — only low nibble should be stored */
    max7219_set_intensity(&dev, 0x0C);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_INTENSITY], 0x0C);

    /* Write 0xFF — driver clamps to 0x0F, model also masks to 0x0F */
    max7219_set_intensity(&dev, 0xFF);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_INTENSITY], 0x0F);
}

/* --- shutdown control -------------------------------------------------- */

TEST(test_shutdown_mode)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    max7219_err_t err = max7219_shutdown(&dev, true);
    ASSERT_EQ(err, MAX7219_OK);
    ASSERT_TRUE(model.shutdown);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_SHUTDOWN], 0x00);
}

TEST(test_shutdown_exit)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    max7219_shutdown(&dev, true);
    ASSERT_TRUE(model.shutdown);

    max7219_shutdown(&dev, false);
    ASSERT_FALSE(model.shutdown);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_SHUTDOWN], 0x01);
}

/* --- display test ------------------------------------------------------ */

TEST(test_display_test_on)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    max7219_err_t err = max7219_display_test(&dev, true);
    ASSERT_EQ(err, MAX7219_OK);
    ASSERT_TRUE(model.display_test);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DISPLAY_TEST], 0x01);
}

TEST(test_display_test_off)
{
    setup();
    max7219_t dev;
    max7219_init(&dev, &model_spi);

    max7219_display_test(&dev, true);
    max7219_display_test(&dev, false);
    ASSERT_FALSE(model.display_test);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_DISPLAY_TEST], 0x00);
}

/* --- register protection ----------------------------------------------- */

TEST(test_shutdown_only_bit0)
{
    setup();
    /* Write 0xFF to shutdown register — only bit 0 should survive */
    hw_model__write_reg(&model.hw, MAX7219_REG_SHUTDOWN, 0xFF);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_SHUTDOWN], 0x01);
}

TEST(test_noop_register_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, MAX7219_REG_NOOP, 0xAB);
    ASSERT_EQ(model.hw.regs[MAX7219_REG_NOOP], 0x00);
}

/* --- the serial interface, from 19-4452 Rev 6 -------------------------- */

static void send_frame(uint8_t high, uint8_t low)
{
    const uint8_t tx[2] = { high, low };

    max7219_model_cs_low(&model);
    (void)max7219_model_spi_transfer(&model, tx, NULL, 2);
    max7219_model_cs_high(&model);
}

/* p.6: "D8-D11 contain the register address. D0-D7 contain the data, and
 * D12-D15 are "don't care" bits." */
TEST(test_d12_to_d15_are_dont_care)
{
    setup();
    send_frame(0x1C, 0x01);
    ASSERT_FALSE(model.shutdown);
    send_frame(0xF1, 0x55);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x55);
}

/* p.6: "The data is then latched into either the digit or control registers
 * on the rising edge of LOAD/CS." */
TEST(test_a_frame_takes_effect_when_load_rises)
{
    const uint8_t tx[2] = { MAX7219_REG_DIGIT0, 0x55 };

    setup();
    max7219_model_cs_low(&model);
    ASSERT_TRUE(max7219_model_spi_transfer(&model, tx, NULL, 2));
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x00);
    max7219_model_cs_high(&model);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x55);
}

/* p.5: "The last 16 bits of serial data are latched on LOAD's rising edge."
 * p.6: it is shifted in "regardless of the state of LOAD". */
TEST(test_only_the_last_16_bits_are_latched)
{
    const uint8_t first[2]  = { MAX7219_REG_DIGIT0, 0x11 };
    const uint8_t second[2] = { MAX7219_REG_DIGIT7, 0x22 };

    setup();
    max7219_model_cs_low(&model);
    (void)max7219_model_spi_transfer(&model, first, NULL, 2);
    (void)max7219_model_spi_transfer(&model, second, NULL, 2);
    max7219_model_cs_high(&model);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT7], 0x22);
}

/* The latch needs an edge, so a frame with LOAD held high does nothing yet */
TEST(test_no_rising_edge_no_latch)
{
    const uint8_t tx[2] = { MAX7219_REG_DIGIT0, 0x55 };

    setup();
    (void)max7219_model_spi_transfer(&model, tx, NULL, 2);
    max7219_model_cs_high(&model);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x00);
    max7219_model_cs_low(&model);
    max7219_model_cs_high(&model);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0], 0x55);
}

/* Table 2: 0xXD and 0xXE are not among the 14 addressable registers */
TEST(test_undefined_addresses_change_nothing)
{
    setup();
    send_frame(0x0D, 0xFF);
    send_frame(0x0E, 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[0x0D], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[0x0E], 0x00);
}

/* p.7: "all control registers are reset, the display is blanked, and the
 * MAX7219/MAX7221 enter shutdown mode", "set to scan one digit, it will not
 * decode data in the data registers, and the intensity register will be set
 * to its minimum value". */
TEST(test_power_up_state)
{
    setup();
    ASSERT_TRUE(model.shutdown);
    ASSERT_FALSE(model.display_test);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_SCAN_LIMIT], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DECODE_MODE], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_INTENSITY], 0x00);
}

/* p.6: "Data in the digit and control registers remains unaltered", and "The
 * display driver can be programmed while in shutdown mode". */
TEST(test_shutdown_keeps_data_and_takes_writes)
{
    setup();
    max7219_t dev;
    ASSERT_EQ(max7219_init(&dev, &model_spi), MAX7219_OK);
    ASSERT_EQ(max7219_set_digit(&dev, 3, 0x7E), MAX7219_OK);

    ASSERT_EQ(max7219_shutdown(&dev, true), MAX7219_OK);
    ASSERT_EQ(max7219_set_digit(&dev, 4, 0x30), MAX7219_OK);
    ASSERT_EQ(max7219_shutdown(&dev, false), MAX7219_OK);

    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0 + 3], 0x7E);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_DIGIT0 + 4], 0x30);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_INTENSITY], 0x08);
}

/* p.10: display test works by "overriding, but not altering, all controls
 * and digit registers (including the shutdown register)". */
TEST(test_display_test_alters_no_register)
{
    setup();
    max7219_t dev;
    ASSERT_EQ(max7219_init(&dev, &model_spi), MAX7219_OK);
    ASSERT_EQ(max7219_shutdown(&dev, true), MAX7219_OK);

    ASSERT_EQ(max7219_display_test(&dev, true), MAX7219_OK);
    ASSERT_TRUE(model.shutdown);
    ASSERT_EQ_HEX(model.hw.regs[MAX7219_REG_SHUTDOWN], 0x00);
}

TEST_MAIN("max7219 hardware model")
    RUN(test_init_exits_shutdown);
    RUN(test_init_disables_display_test);
    RUN(test_init_sets_scan_limit_all);
    RUN(test_init_sets_no_decode);
    RUN(test_init_sets_default_intensity);
    RUN(test_init_clears_all_digits);
    RUN(test_set_digit_writes_register);
    RUN(test_intensity_register_masked);
    RUN(test_shutdown_mode);
    RUN(test_shutdown_exit);
    RUN(test_display_test_on);
    RUN(test_display_test_off);
    RUN(test_shutdown_only_bit0);
    RUN(test_noop_register_readonly);
    RUN(test_d12_to_d15_are_dont_care);
    RUN(test_a_frame_takes_effect_when_load_rises);
    RUN(test_only_the_last_16_bits_are_latched);
    RUN(test_no_rising_edge_no_latch);
    RUN(test_undefined_addresses_change_nothing);
    RUN(test_power_up_state);
    RUN(test_shutdown_keeps_data_and_takes_writes);
    RUN(test_display_test_alters_no_register);
TEST_MAIN_END()
