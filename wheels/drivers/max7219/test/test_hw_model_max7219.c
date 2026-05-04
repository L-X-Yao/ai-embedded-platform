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
TEST_MAIN_END()
