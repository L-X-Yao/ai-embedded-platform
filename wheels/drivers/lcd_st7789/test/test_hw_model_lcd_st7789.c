// SPDX-License-Identifier: MIT
/*
 * test_hw_model_lcd_st7789.c -- tests using ST7789 SPI display model
 *
 * Validates the lcd_st7789 driver against a datasheet-accurate command
 * processor model that tracks internal display state:
 *   sleep mode, display on/off, inversion, MADCTL rotation, COLMOD,
 *   column/row address window, pixel data, command sequencing.
 *
 * Reference: Sitronix ST7789V datasheet rev 1.6
 */

#include "test_harness.h"
#include "hw_model_lcd_st7789.h"
#include "../lcd_st7789.h"

static lcd_st7789_model_t model;
static lcd_st7789_spi_t   model_spi;
static lcd_st7789_t       dev;

static const lcd_st7789_cfg_t default_cfg = {
    .width      = 240,
    .height     = 320,
    .col_offset = 0,
    .row_offset = 0,
};

static lcd_st7789_err_t setup(void)
{
    lcd_st7789_model_init(&model);
    model_spi = st7789_model_spi(&model);
    return lcd_st7789_init(&dev, &model_spi, &default_cfg);
}

/* ====================================================================
 * 1. Init sequence -- sleep / display state
 * ==================================================================== */

TEST(test_init_exits_sleep)
{
    /* After init the display must have exited sleep mode (SLPOUT 0x11) */
    setup();
    ASSERT_TRUE(st7789_model_is_awake(&model));
}

TEST(test_init_display_on)
{
    /* Init sequence must end with DISPON (0x29) */
    setup();
    ASSERT_TRUE(st7789_model_is_display_on(&model));
}

TEST(test_init_no_sequence_violations)
{
    /* DISPON must occur after SLPOUT -- no violations expected */
    setup();
    ASSERT_EQ(model.seq_violations, 0);
}

/* ====================================================================
 * 2. Init sequence -- COLMOD / MADCTL
 * ==================================================================== */

TEST(test_init_sets_rgb565)
{
    /* Driver must set COLMOD to 0x55 (RGB565) during init */
    setup();
    ASSERT_EQ_HEX(model.colmod, LCD_ST7789_COLMOD_RGB565);
}

TEST(test_init_madctl_rot0)
{
    /* Rotation 0: MADCTL = 0x00 (no MY/MX/MV, RGB order) */
    setup();
    ASSERT_EQ_HEX(model.madctl, 0x00);
}

/* ====================================================================
 * 3. Init sequence -- inversion
 * ==================================================================== */

TEST(test_init_enables_inversion)
{
    /* ST7789 typically needs INVON for correct colors -- driver does this */
    setup();
    ASSERT_TRUE(model.inversion);
}

/* ====================================================================
 * 4. Init sequence -- hardware reset
 * ==================================================================== */

TEST(test_init_toggles_rst)
{
    /* Driver must assert RST (low) then deassert (high) during HW reset */
    setup();
    ASSERT_TRUE(model.rst_toggle_count >= 1);
    /* RST should end high (normal operation) */
    ASSERT_TRUE(model.rst_level);
}

TEST(test_init_sufficient_delays)
{
    /* Datasheet requires: HW reset 10+120ms, SWRESET 150ms, SLPOUT 120ms,
     * DISPON ~20ms. Total >= 420ms. */
    setup();
    ASSERT_TRUE(model.delay_total_ms >= 420);
    ASSERT_TRUE(model.delay_call_count >= 4);
}

/* ====================================================================
 * 5. Rotation -- MADCTL values per datasheet
 * ==================================================================== */

TEST(test_rotation_0_madctl)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_0);
    /* ROT_0: MY=0, MX=0, MV=0 => 0x00 | RGB=0x00 => 0x00 */
    ASSERT_EQ_HEX(model.madctl, 0x00);
}

TEST(test_rotation_90_madctl)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    /* ROT_90: MV=1, MY=1 => 0x20|0x80 = 0xA0 */
    ASSERT_EQ_HEX(model.madctl, 0xA0);
}

TEST(test_rotation_180_madctl)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_180);
    /* ROT_180: MX=1, MY=1 => 0x40|0x80 = 0xC0 */
    ASSERT_EQ_HEX(model.madctl, 0xC0);
}

TEST(test_rotation_270_madctl)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_270);
    /* ROT_270: MV=1, MX=1 => 0x20|0x40 = 0x60 */
    ASSERT_EQ_HEX(model.madctl, 0x60);
}

/* ====================================================================
 * 6. Window coordinates -- CASET / RASET
 * ==================================================================== */

TEST(test_set_window_updates_model_caset_raset)
{
    setup();
    lcd_st7789_err_t err = lcd_st7789_set_window(&dev, 10, 20, 50, 60);
    ASSERT_EQ(err, LCD_ST7789_OK);
    ASSERT_EQ(model.col_start, 10);
    ASSERT_EQ(model.col_end,   50);
    ASSERT_EQ(model.row_start, 20);
    ASSERT_EQ(model.row_end,   60);
}

TEST(test_set_window_full_screen)
{
    setup();
    lcd_st7789_set_window(&dev, 0, 0, 239, 319);
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.col_end,   239);
    ASSERT_EQ(model.row_start, 0);
    ASSERT_EQ(model.row_end,   319);
}

TEST(test_set_window_with_offset)
{
    /* A 240x240 panel with row_offset=80 (e.g. 1.3" IPS module) */
    lcd_st7789_model_init(&model);
    model_spi = st7789_model_spi(&model);
    lcd_st7789_cfg_t cfg_off = { .width = 240, .height = 240,
                                  .col_offset = 0, .row_offset = 80 };
    lcd_st7789_t d;
    lcd_st7789_init(&d, &model_spi, &cfg_off);

    lcd_st7789_set_window(&d, 0, 0, 10, 10);
    /* Model should see offset applied: row 0+80=80, row 10+80=90 */
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.col_end,   10);
    ASSERT_EQ(model.row_start, 80);
    ASSERT_EQ(model.row_end,   90);
}

/* ====================================================================
 * 7. Pixel data -- RAMWR tracking
 * ==================================================================== */

TEST(test_write_pixels_reaches_model)
{
    setup();
    lcd_st7789_set_window(&dev, 0, 0, 1, 0);
    /* 2 pixels = 4 bytes RGB565 */
    uint8_t px[4] = { 0xF8, 0x00, 0x07, 0xE0 };
    lcd_st7789_err_t err = lcd_st7789_write_pixels(&dev, px, 4);
    ASSERT_EQ(err, LCD_ST7789_OK);
    ASSERT_EQ(model.pixel_buf_pos, 4);
    ASSERT_EQ(model.pixel_buf[0], 0xF8);
    ASSERT_EQ(model.pixel_buf[1], 0x00);
    ASSERT_EQ(model.pixel_buf[2], 0x07);
    ASSERT_EQ(model.pixel_buf[3], 0xE0);
}

TEST(test_fill_rect_pixel_count)
{
    setup();
    /* Fill a 3x2 rect: 6 pixels x 2 bytes = 12 bytes of pixel data */
    lcd_st7789_err_t err = lcd_st7789_fill_rect(&dev, 0, 0, 3, 2, 0xF800);
    ASSERT_EQ(err, LCD_ST7789_OK);
    ASSERT_EQ(model.pixel_buf_pos, 12);
    /* Each pixel should be 0xF8, 0x00 (red in RGB565 big-endian wire) */
    ASSERT_EQ(model.pixel_buf[0], 0xF8);
    ASSERT_EQ(model.pixel_buf[1], 0x00);
    ASSERT_EQ(model.pixel_buf[10], 0xF8);
    ASSERT_EQ(model.pixel_buf[11], 0x00);
}

TEST(test_fill_rect_sets_window_then_writes)
{
    setup();
    lcd_st7789_fill_rect(&dev, 5, 10, 20, 30, 0x001F);
    /* Window should be (5, 10) to (24, 39) */
    ASSERT_EQ(model.col_start, 5);
    ASSERT_EQ(model.col_end,   24);
    ASSERT_EQ(model.row_start, 10);
    ASSERT_EQ(model.row_end,   39);
    /* 20 * 30 = 600 pixels * 2 bytes = 1200 bytes */
    ASSERT_EQ(model.pixel_buf_pos, 1200);
}

/* ====================================================================
 * 8. Display on/off round-trip
 * ==================================================================== */

TEST(test_display_off_on_round_trip)
{
    setup();
    ASSERT_TRUE(model.display_on);

    lcd_st7789_display_off(&dev);
    ASSERT_FALSE(model.display_on);

    lcd_st7789_display_on(&dev);
    ASSERT_TRUE(model.display_on);
}

/* ====================================================================
 * 9. Inversion toggle
 * ==================================================================== */

TEST(test_invert_toggle)
{
    setup();
    /* After init, inversion is ON (driver sends INVON) */
    ASSERT_TRUE(model.inversion);

    lcd_st7789_invert(&dev, false);
    ASSERT_FALSE(model.inversion);

    lcd_st7789_invert(&dev, true);
    ASSERT_TRUE(model.inversion);
}

/* ====================================================================
 * 10. Rotation updates effective dimensions in driver
 * ==================================================================== */

TEST(test_rotation_90_swaps_dimensions)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    ASSERT_EQ(dev.cur_width, 320);
    ASSERT_EQ(dev.cur_height, 240);
}

TEST(test_rotation_180_keeps_dimensions)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_180);
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
}

/* ====================================================================
 * 11. Window after rotation uses swapped offsets
 * ==================================================================== */

TEST(test_window_after_rotation_90_offset)
{
    lcd_st7789_model_init(&model);
    model_spi = st7789_model_spi(&model);
    lcd_st7789_cfg_t cfg_off = { .width = 240, .height = 240,
                                  .col_offset = 0, .row_offset = 80 };
    lcd_st7789_t d;
    lcd_st7789_init(&d, &model_spi, &cfg_off);
    lcd_st7789_set_rotation(&d, LCD_ST7789_ROT_90);

    /* After 90-degree rotation, col_offset and row_offset should swap */
    lcd_st7789_set_window(&d, 0, 0, 10, 10);
    ASSERT_EQ(model.col_start, 80);  /* was row_offset, now col_offset */
    ASSERT_EQ(model.col_end,   90);
    ASSERT_EQ(model.row_start, 0);
    ASSERT_EQ(model.row_end,   10);
}

/* ====================================================================
 * 12. SWRESET during init resets model to POR
 * ==================================================================== */

TEST(test_swreset_returns_to_por_then_reinits)
{
    /* Init sends SWRESET which resets model, then continues config.
     * Verify the final state is fully configured (not stuck in POR). */
    setup();
    ASSERT_FALSE(model.sleep);        /* SLPOUT was sent after SWRESET */
    ASSERT_TRUE(model.display_on);    /* DISPON was sent */
    ASSERT_EQ_HEX(model.colmod, 0x55); /* RGB565 was set */
}

/* ====================================================================
 * 13. Command count sanity
 * ==================================================================== */

TEST(test_init_sends_multiple_commands)
{
    setup();
    /* Init sequence: SWRESET, SLPOUT, COLMOD, MADCTL, INVON, DISPON = 6 min */
    ASSERT_TRUE(model.cmd_count >= 6);
}

/* ====================================================================
 * Runner
 * ==================================================================== */

TEST(test_swreset_keeps_madctl_and_colmod)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    uint8_t madctl = model.madctl;

    lcd_st7789_model_sw_reset(&model);

    ASSERT_EQ_HEX(model.madctl, madctl);
    ASSERT_EQ_HEX(model.colmod, LCD_ST7789_COLMOD_RGB565);
    /* MV=1 swaps the default address window */
    ASSERT_EQ(model.col_end, 0x013F);
    ASSERT_EQ(model.row_end, 0x00EF);
}

TEST_MAIN("lcd_st7789 hardware model")
    /* init sequence */
    RUN(test_init_exits_sleep);
    RUN(test_init_display_on);
    RUN(test_init_no_sequence_violations);
    RUN(test_init_sets_rgb565);
    RUN(test_init_madctl_rot0);
    RUN(test_init_enables_inversion);
    RUN(test_init_toggles_rst);
    RUN(test_init_sufficient_delays);
    /* rotation MADCTL */
    RUN(test_rotation_0_madctl);
    RUN(test_rotation_90_madctl);
    RUN(test_rotation_180_madctl);
    RUN(test_rotation_270_madctl);
    /* window coordinates */
    RUN(test_set_window_updates_model_caset_raset);
    RUN(test_set_window_full_screen);
    RUN(test_set_window_with_offset);
    /* pixel data */
    RUN(test_write_pixels_reaches_model);
    RUN(test_fill_rect_pixel_count);
    RUN(test_fill_rect_sets_window_then_writes);
    /* display on/off */
    RUN(test_display_off_on_round_trip);
    /* inversion */
    RUN(test_invert_toggle);
    /* rotation + dimensions */
    RUN(test_rotation_90_swaps_dimensions);
    RUN(test_rotation_180_keeps_dimensions);
    /* rotation + offsets */
    RUN(test_window_after_rotation_90_offset);
    /* SWRESET behavior */
    RUN(test_swreset_returns_to_por_then_reinits);
    /* command count */
    RUN(test_init_sends_multiple_commands);
    RUN(test_swreset_keeps_madctl_and_colmod);
TEST_MAIN_END()
