// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "sh1106.h"
#include <string.h>

/* ---------- chip-specific mock setup ---------- */

static uint8_t last_reg;
static uint8_t last_val;
static int cmd_count;
static int data_before_on;
static bool on_seen;

/* Override to track command/data writes via control byte */
static bool track_write_reg(void *ctx, uint8_t dev_addr,
                            uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    last_reg = reg;
    last_val = val;
    cmd_count++;
    if (reg == 0x40 && !on_seen) data_before_on++;
    if (reg == 0x00 && val == 0xAF) on_seen = true;
    return true;
}

static void mock_reset(void)
{
    mock_i2c_reset();
    last_reg = 0;
    last_val = 0;
    cmd_count = 0;
    data_before_on = 0;
    on_seen = false;
}

static const sh1106_i2c_t mock_i2c = {
    .write_reg = track_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static sh1106_t dev;
static uint8_t framebuf[SH1106_BUF_SIZE];

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    sh1106_err_t e = sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    ASSERT_EQ(e, SH1106_OK);
    /* Should have sent many commands during init */
    ASSERT_TRUE(cmd_count > 10);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    sh1106_err_t e = sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    ASSERT_EQ(e, SH1106_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(sh1106_init(NULL, &mock_i2c, SH1106_ADDR_A, framebuf), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_init(&dev, NULL, SH1106_ADDR_A, framebuf), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, NULL), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_display(NULL), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_set_contrast(NULL, 0x80), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_display_on(NULL), SH1106_ERR_PARAM);
    ASSERT_EQ(sh1106_display_off(NULL), SH1106_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    sh1106_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    sh1106_t zero_dev;
    ASSERT_TRUE(sh1106_init(&zero_dev, &zero_hal, SH1106_ADDR_A, framebuf) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)SH1106_ERR_I2C);
    ASSERT_EQ(-2, (int)SH1106_ERR_ID);
    ASSERT_EQ(-3, (int)SH1106_ERR_PARAM);
}

TEST(test_set_get_pixel)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    ASSERT_FALSE(sh1106_get_pixel(&dev, 10, 20));
    sh1106_set_pixel(&dev, 10, 20, true);
    ASSERT_TRUE(sh1106_get_pixel(&dev, 10, 20));
    sh1106_set_pixel(&dev, 10, 20, false);
    ASSERT_FALSE(sh1106_get_pixel(&dev, 10, 20));
}

TEST(test_pixel_out_of_bounds)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    sh1106_set_pixel(&dev, 128, 0, true);
    sh1106_set_pixel(&dev, 0, 64, true);
    sh1106_set_pixel(&dev, 255, 255, true);
    ASSERT_FALSE(sh1106_get_pixel(&dev, 128, 0));
    ASSERT_FALSE(sh1106_get_pixel(&dev, 0, 64));
}

TEST(test_clear)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    sh1106_set_pixel(&dev, 0, 0, true);
    sh1106_set_pixel(&dev, 64, 32, true);
    sh1106_clear(&dev);
    ASSERT_FALSE(sh1106_get_pixel(&dev, 0, 0));
    ASSERT_FALSE(sh1106_get_pixel(&dev, 64, 32));
}

TEST(test_fill)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    sh1106_fill(&dev, true);
    ASSERT_TRUE(sh1106_get_pixel(&dev, 0, 0));
    ASSERT_TRUE(sh1106_get_pixel(&dev, 127, 63));

    sh1106_fill(&dev, false);
    ASSERT_FALSE(sh1106_get_pixel(&dev, 0, 0));
}

TEST(test_pixel_bit_packing)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    for (uint8_t bit = 0; bit < 8; bit++)
    {
        sh1106_set_pixel(&dev, 0, bit, true);
    }
    ASSERT_EQ(framebuf[0], 0xFF);
}

TEST(test_multiple_pixels_same_byte)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    sh1106_set_pixel(&dev, 5, 0, true);
    sh1106_set_pixel(&dev, 5, 3, true);
    ASSERT_TRUE(sh1106_get_pixel(&dev, 5, 0));
    ASSERT_TRUE(sh1106_get_pixel(&dev, 5, 3));
    ASSERT_FALSE(sh1106_get_pixel(&dev, 5, 1));
}

TEST(test_display_flushes_all_pages)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    mock_reset();

    sh1106_err_t e = sh1106_display(&dev);
    ASSERT_EQ(e, SH1106_OK);
    /* 8 pages * (3 commands + 128 data bytes) = 8 * 131 = 1048 writes */
    ASSERT_TRUE(cmd_count > 1000);
}

TEST(test_display_i2c_fail)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(sh1106_display(&dev), SH1106_ERR_I2C);
}

TEST(test_set_contrast)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    mock_reset();

    sh1106_err_t e = sh1106_set_contrast(&dev, 0xFF);
    ASSERT_EQ(e, SH1106_OK);
    ASSERT_EQ(last_val, 0xFF);
}

TEST(test_set_contrast_i2c_fail)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(sh1106_set_contrast(&dev, 0x80), SH1106_ERR_I2C);
}

TEST(test_display_on_off)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);

    mock_reset();
    ASSERT_EQ(sh1106_display_on(&dev), SH1106_OK);
    /* last command sent via write_reg with control byte 0x00 and value 0xAF */
    ASSERT_EQ(last_val, 0xAF);

    mock_reset();
    ASSERT_EQ(sh1106_display_off(&dev), SH1106_OK);
    ASSERT_EQ(last_val, 0xAE);
}

TEST(test_display_on_off_i2c_fail)
{
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(sh1106_display_on(&dev), SH1106_ERR_I2C);

    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(sh1106_display_off(&dev), SH1106_ERR_I2C);
}

TEST(test_null_clear_fill_pixel)
{
    sh1106_clear(NULL);
    sh1106_fill(NULL, true);
    sh1106_set_pixel(NULL, 0, 0, true);
    ASSERT_FALSE(sh1106_get_pixel(NULL, 0, 0));
    ASSERT_TRUE(1);
}

TEST(test_addr_b)
{
    mock_reset();
    sh1106_err_t e = sh1106_init(&dev, &mock_i2c, SH1106_ADDR_B, framebuf);
    ASSERT_EQ(e, SH1106_OK);
}

TEST(test_init_clears_framebuffer)
{
    memset(framebuf, 0xFF, sizeof(framebuf));
    mock_reset();
    sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf);
    /* Init should clear the framebuffer */
    ASSERT_FALSE(sh1106_get_pixel(&dev, 0, 0));
    ASSERT_FALSE(sh1106_get_pixel(&dev, 127, 63));
}

TEST(test_init_clears_display_ram_before_switching_on)
{
    mock_reset();
    ASSERT_EQ(sh1106_init(&dev, &mock_i2c, SH1106_ADDR_A, framebuf), SH1106_OK);

    ASSERT_TRUE(on_seen);
    ASSERT_EQ(data_before_on, SH1106_PAGES * SH1106_WIDTH);
}

TEST_MAIN("sh1106")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_set_get_pixel);
    RUN(test_pixel_out_of_bounds);
    RUN(test_clear);
    RUN(test_fill);
    RUN(test_pixel_bit_packing);
    RUN(test_multiple_pixels_same_byte);
    RUN(test_display_flushes_all_pages);
    RUN(test_display_i2c_fail);
    RUN(test_set_contrast);
    RUN(test_set_contrast_i2c_fail);
    RUN(test_display_on_off);
    RUN(test_display_on_off_i2c_fail);
    RUN(test_null_clear_fill_pixel);
    RUN(test_addr_b);
    RUN(test_init_clears_framebuffer);
    RUN(test_init_clears_display_ram_before_switching_on);
TEST_MAIN_END()
