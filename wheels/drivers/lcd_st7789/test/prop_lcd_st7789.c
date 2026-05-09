// SPDX-License-Identifier: MIT
/*
 * prop_lcd_st7789.c — property-based tests for lcd_st7789
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_lcd_st7789.c ../lcd_st7789.c -o prop_lcd_st7789
 */

#include "test_harness.h"
#include "property_test.h"
#include "lcd_st7789.h"
#include <string.h>

/* ---------- mock SPI ---------- */

static void mock_cs_low(void *ctx)  { (void)ctx; }
static void mock_cs_high(void *ctx) { (void)ctx; }

static void mock_dc_set(void *ctx, bool level)
{
    (void)ctx; (void)level;
}

static void mock_rst_set(void *ctx, bool level)
{
    (void)ctx; (void)level;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    return 0xFF;
}

static const lcd_st7789_spi_t prop_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .dc_set   = mock_dc_set,
    .rst_set  = mock_rst_set,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static const lcd_st7789_cfg_t default_cfg = {
    .width      = 240,
    .height     = 320,
    .col_offset = 0,
    .row_offset = 0,
};

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;

    lcd_st7789_t dev;
    lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    /* Must not crash */
}

PROPERTY(rotation_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;

    lcd_st7789_t dev;
    lcd_st7789_init(&dev, &prop_spi, &default_cfg);

    uint8_t rot = ARBI_UINT8(seed);
    lcd_st7789_set_rotation(&dev, (lcd_st7789_rotation_t)rot);
    /* Must not crash even with invalid rotation */
}

PROPERTY(rotation_swaps_dims, 5000)
{
    lcd_st7789_t dev;
    lcd_st7789_err_t err = lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    if (err != LCD_ST7789_OK) return;

    lcd_st7789_rotation_t rot = (lcd_st7789_rotation_t)(ARBI_UINT8(seed) % 4);
    err = lcd_st7789_set_rotation(&dev, rot);
    PROP_ASSERT(err == LCD_ST7789_OK);

    if (rot == LCD_ST7789_ROT_0 || rot == LCD_ST7789_ROT_180) {
        PROP_ASSERT_EQ(dev.cur_width, 240);
        PROP_ASSERT_EQ(dev.cur_height, 320);
    } else {
        PROP_ASSERT_EQ(dev.cur_width, 320);
        PROP_ASSERT_EQ(dev.cur_height, 240);
    }
}

PROPERTY(set_window_valid_no_crash, 5000)
{
    lcd_st7789_t dev;
    lcd_st7789_err_t err = lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    if (err != LCD_ST7789_OK) return;

    /* Generate valid coordinates */
    uint16_t x0 = ARBI_UINT16(seed) % dev.cur_width;
    uint16_t y0 = ARBI_UINT16(seed) % dev.cur_height;
    uint16_t x1 = x0 + (ARBI_UINT16(seed) % (dev.cur_width - x0));
    uint16_t y1 = y0 + (ARBI_UINT16(seed) % (dev.cur_height - y0));

    err = lcd_st7789_set_window(&dev, x0, y0, x1, y1);
    PROP_ASSERT(err == LCD_ST7789_OK);
}

PROPERTY(fill_rect_valid_no_crash, 5000)
{
    lcd_st7789_t dev;
    lcd_st7789_err_t err = lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    if (err != LCD_ST7789_OK) return;

    /* Generate valid region */
    uint16_t x = ARBI_UINT16(seed) % dev.cur_width;
    uint16_t y = ARBI_UINT16(seed) % dev.cur_height;
    uint16_t w = (uint16_t)(ARBI_UINT16(seed) % (dev.cur_width - x) + 1);
    uint16_t h = (uint16_t)(ARBI_UINT16(seed) % (dev.cur_height - y) + 1);
    uint16_t color = ARBI_UINT16(seed);

    err = lcd_st7789_fill_rect(&dev, x, y, w, h, color);
    PROP_ASSERT(err == LCD_ST7789_OK);
}

PROPERTY(config_variants, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;

    lcd_st7789_t dev;
    lcd_st7789_cfg_t cfg = {
        .width      = (uint16_t)(ARBI_UINT16(seed) % 400 + 1),
        .height     = (uint16_t)(ARBI_UINT16(seed) % 400 + 1),
        .col_offset = ARBI_UINT16(seed),
        .row_offset = ARBI_UINT16(seed),
    };

    lcd_st7789_init(&dev, &prop_spi, &cfg);
    /* Must not crash with various panel configs */
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;

    lcd_st7789_t dev;
    lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    /* Must not crash */
}

PROPERTY(invert_toggle_no_crash, 5000)
{
    lcd_st7789_t dev;
    lcd_st7789_err_t err = lcd_st7789_init(&dev, &prop_spi, &default_cfg);
    if (err != LCD_ST7789_OK) return;

    bool on = (ARBI_UINT8(seed) & 1) != 0;
    err = lcd_st7789_invert(&dev, on);
    PROP_ASSERT(err == LCD_ST7789_OK);
}

TEST_MAIN("lcd_st7789 properties")
    RUN(init_no_crash);
    RUN(rotation_no_crash);
    RUN(rotation_swaps_dims);
    RUN(set_window_valid_no_crash);
    RUN(fill_rect_valid_no_crash);
    RUN(config_variants);
    RUN(double_init_safe);
    RUN(invert_toggle_no_crash);
TEST_MAIN_END()
