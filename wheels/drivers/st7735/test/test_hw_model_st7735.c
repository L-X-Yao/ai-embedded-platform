// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_st7735.h"
#include "st7735.h"

static st7735_spi_t model_spi;
static st7735_t     dev;

static st7735_err_t setup(void)
{
    st7735_model_init();
    model_spi = st7735_model_spi();
    return st7735_init(&dev, &model_spi);
}

/* ====================================================================
 * Init sequence tests
 * ==================================================================== */

TEST(test_init_exits_sleep)
{
    setup();
    ASSERT_FALSE(st7735_mdl.sleep);
}

TEST(test_init_display_on)
{
    setup();
    ASSERT_TRUE(st7735_mdl.display_on);
}

TEST(test_init_sets_rgb565)
{
    setup();
    /* COLMOD 0x05 = RGB565 (16-bit) */
    ASSERT_EQ_HEX(st7735_mdl.colmod, 0x05);
}

TEST(test_init_normal_mode)
{
    setup();
    ASSERT_TRUE(st7735_mdl.normal_mode);
}

TEST(test_init_hw_reset)
{
    setup();
    ASSERT_TRUE(st7735_mdl.reset_count >= 1);
}

TEST(test_init_sufficient_delays)
{
    setup();
    /* Need at least reset delay + sleep out delay */
    ASSERT_TRUE(st7735_mdl.delay_total_ms >= 240);
    ASSERT_TRUE(st7735_mdl.delay_call_count >= 3);
}

/* ====================================================================
 * Window tests
 * ==================================================================== */

TEST(test_set_window_updates_model)
{
    setup();
    st7735_err_t err = st7735_set_window(&dev, 10, 20, 50, 60);
    ASSERT_EQ(err, ST7735_OK);
    ASSERT_EQ(st7735_mdl.col_start, 10);
    ASSERT_EQ(st7735_mdl.col_end,   50);
    ASSERT_EQ(st7735_mdl.row_start, 20);
    ASSERT_EQ(st7735_mdl.row_end,   60);
}

TEST(test_set_window_full_screen)
{
    setup();
    st7735_set_window(&dev, 0, 0, 127, 159);
    ASSERT_EQ(st7735_mdl.col_start, 0);
    ASSERT_EQ(st7735_mdl.col_end,   127);
    ASSERT_EQ(st7735_mdl.row_start, 0);
    ASSERT_EQ(st7735_mdl.row_end,   159);
}

/* ====================================================================
 * Pixel data tests
 * ==================================================================== */

TEST(test_write_pixels_reaches_model)
{
    setup();
    st7735_set_window(&dev, 0, 0, 1, 0);
    uint16_t px[2] = { 0xF800, 0x07E0 };
    st7735_err_t err = st7735_write_pixels(&dev, px, 2);
    ASSERT_EQ(err, ST7735_OK);
    ASSERT_EQ(st7735_mdl.pixel_buf_pos, 4);
    ASSERT_EQ(st7735_mdl.pixel_buf[0], 0xF8);
    ASSERT_EQ(st7735_mdl.pixel_buf[1], 0x00);
    ASSERT_EQ(st7735_mdl.pixel_buf[2], 0x07);
    ASSERT_EQ(st7735_mdl.pixel_buf[3], 0xE0);
}

TEST(test_fill_rect_pixel_count)
{
    setup();
    /* 3x2 = 6 pixels * 2 bytes = 12 bytes */
    st7735_err_t err = st7735_fill_rect(&dev, 0, 0, 3, 2, 0xF800);
    ASSERT_EQ(err, ST7735_OK);
    ASSERT_EQ(st7735_mdl.pixel_buf_pos, 12);
    /* Each pixel: 0xF8, 0x00 */
    ASSERT_EQ(st7735_mdl.pixel_buf[0], 0xF8);
    ASSERT_EQ(st7735_mdl.pixel_buf[1], 0x00);
}

TEST(test_fill_rect_sets_window_then_writes)
{
    setup();
    st7735_fill_rect(&dev, 5, 10, 20, 30, 0x001F);
    ASSERT_EQ(st7735_mdl.col_start, 5);
    ASSERT_EQ(st7735_mdl.col_end,   24);
    ASSERT_EQ(st7735_mdl.row_start, 10);
    ASSERT_EQ(st7735_mdl.row_end,   39);
    /* 20 * 30 = 600 pixels * 2 = 1200 bytes */
    ASSERT_EQ(st7735_mdl.pixel_buf_pos, 1200);
}

TEST(test_draw_pixel_single)
{
    setup();
    st7735_draw_pixel(&dev, 64, 80, 0xABCD);
    ASSERT_EQ(st7735_mdl.col_start, 64);
    ASSERT_EQ(st7735_mdl.col_end,   64);
    ASSERT_EQ(st7735_mdl.row_start, 80);
    ASSERT_EQ(st7735_mdl.row_end,   80);
    ASSERT_EQ(st7735_mdl.pixel_buf_pos, 2);
    ASSERT_EQ(st7735_mdl.pixel_buf[0], 0xAB);
    ASSERT_EQ(st7735_mdl.pixel_buf[1], 0xCD);
}

TEST(test_command_count)
{
    setup();
    /* At minimum: SLPOUT, COLMOD, MADCTL, NORON, DISPON = 5 commands */
    ASSERT_TRUE(st7735_mdl.cmd_count >= 5);
}

TEST_MAIN("st7735_hw_model")
    RUN(test_init_exits_sleep);
    RUN(test_init_display_on);
    RUN(test_init_sets_rgb565);
    RUN(test_init_normal_mode);
    RUN(test_init_hw_reset);
    RUN(test_init_sufficient_delays);
    RUN(test_set_window_updates_model);
    RUN(test_set_window_full_screen);
    RUN(test_write_pixels_reaches_model);
    RUN(test_fill_rect_pixel_count);
    RUN(test_fill_rect_sets_window_then_writes);
    RUN(test_draw_pixel_single);
    RUN(test_command_count);
TEST_MAIN_END()
