// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "lcd_st7789.h"

/* ---------- mock SPI ---------- */

#define CMD_LOG_SIZE  512
#define DATA_LOG_SIZE 4096

static uint8_t cmd_log[CMD_LOG_SIZE];
static int     cmd_log_idx;
static uint8_t data_log[DATA_LOG_SIZE];
static int     data_log_idx;
static bool    dc_state;       /* false=cmd, true=data */
static bool    cs_state;       /* false=low(asserted), true=high */
static bool    rst_state;      /* false=reset, true=normal */
static int     xfer_count;
static int     delay_total_ms;
static int     delay_call_count;

static void mock_reset(void)
{
    memset(cmd_log, 0, sizeof(cmd_log));
    cmd_log_idx    = 0;
    memset(data_log, 0, sizeof(data_log));
    data_log_idx   = 0;
    dc_state       = true;
    cs_state       = true;
    rst_state      = true;
    xfer_count     = 0;
    delay_total_ms = 0;
    delay_call_count = 0;
}

static void mock_cs_low(void *ctx)   { (void)ctx; cs_state = false; }
static void mock_cs_high(void *ctx)  { (void)ctx; cs_state = true;  }

static void mock_dc_set(void *ctx, bool level)
{
    (void)ctx;
    dc_state = level;
}

static void mock_rst_set(void *ctx, bool level)
{
    (void)ctx;
    rst_state = level;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    delay_total_ms += (int)ms;
    delay_call_count++;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    xfer_count++;
    if (!dc_state) {
        /* command mode */
        if (cmd_log_idx < CMD_LOG_SIZE) {
            cmd_log[cmd_log_idx++] = tx;
        }
    } else {
        /* data mode */
        if (data_log_idx < DATA_LOG_SIZE) {
            data_log[data_log_idx++] = tx;
        }
    }
    return 0xFF;
}

static const lcd_st7789_spi_t mock_spi = {
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

static lcd_st7789_t dev;

static lcd_st7789_err_t setup(void)
{
    mock_reset();
    return lcd_st7789_init(&dev, &mock_spi, &default_cfg);
}

/* ---------- helper: find cmd in cmd_log ---------- */

static int find_cmd(uint8_t cmd)
{
    for (int i = 0; i < cmd_log_idx; i++) {
        if (cmd_log[i] == cmd) return i;
    }
    return -1;
}

/* ========== init tests ========== */

TEST(test_init_ok)
{
    lcd_st7789_err_t e = setup();
    ASSERT_EQ(e, LCD_ST7789_OK);
}

TEST(test_init_sends_reset_sequence)
{
    setup();
    /* delay_ms must be called for HW reset + SW reset + sleep out + display on */
    ASSERT_TRUE(delay_call_count >= 4);
    /* total delay should be at least 10+120+150+120+20 = 420ms */
    ASSERT_TRUE(delay_total_ms >= 420);
}

TEST(test_init_sends_swreset)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_SWRESET) >= 0);
}

TEST(test_init_sends_slpout)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_SLPOUT) >= 0);
}

TEST(test_init_sends_colmod)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_COLMOD) >= 0);
}

TEST(test_init_sends_madctl)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_MADCTL) >= 0);
}

TEST(test_init_sends_invon)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_INVON) >= 0);
}

TEST(test_init_sends_dispon)
{
    setup();
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_DISPON) >= 0);
}

TEST(test_init_sets_dimensions)
{
    setup();
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
    ASSERT_EQ(dev.rotation, LCD_ST7789_ROT_0);
}

/* ========== null / param safety tests ========== */

TEST(test_init_null_dev)
{
    ASSERT_EQ(lcd_st7789_init(NULL, &mock_spi, &default_cfg),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    lcd_st7789_t d;
    ASSERT_EQ(lcd_st7789_init(&d, NULL, &default_cfg),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    lcd_st7789_t d;
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, NULL),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_zero_width)
{
    lcd_st7789_t d;
    lcd_st7789_cfg_t bad = { .width = 0, .height = 320, .col_offset = 0, .row_offset = 0 };
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, &bad),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_zero_height)
{
    lcd_st7789_t d;
    lcd_st7789_cfg_t bad = { .width = 240, .height = 0, .col_offset = 0, .row_offset = 0 };
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, &bad),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_missing_hal_func)
{
    lcd_st7789_t d;
    /* missing dc_set */
    lcd_st7789_spi_t bad_spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .dc_set   = NULL,
        .rst_set  = mock_rst_set,
        .delay_ms = mock_delay_ms,
        .ctx      = NULL,
    };
    ASSERT_EQ(lcd_st7789_init(&d, &bad_spi, &default_cfg),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_missing_rst_func)
{
    lcd_st7789_t d;
    lcd_st7789_spi_t bad_spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .dc_set   = mock_dc_set,
        .rst_set  = NULL,
        .delay_ms = mock_delay_ms,
        .ctx      = NULL,
    };
    ASSERT_EQ(lcd_st7789_init(&d, &bad_spi, &default_cfg),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_init_missing_delay_func)
{
    lcd_st7789_t d;
    lcd_st7789_spi_t bad_spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .dc_set   = mock_dc_set,
        .rst_set  = mock_rst_set,
        .delay_ms = NULL,
        .ctx      = NULL,
    };
    ASSERT_EQ(lcd_st7789_init(&d, &bad_spi, &default_cfg),
              LCD_ST7789_ERR_PARAM);
}

/* ========== set_window tests ========== */

TEST(test_set_window_ok)
{
    setup();
    mock_reset();
    lcd_st7789_err_t e = lcd_st7789_set_window(&dev, 0, 0, 239, 319);
    ASSERT_EQ(e, LCD_ST7789_OK);
    /* Should have sent CASET and RASET */
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_RASET) >= 0);
}

TEST(test_set_window_small_region)
{
    setup();
    mock_reset();
    lcd_st7789_err_t e = lcd_st7789_set_window(&dev, 10, 20, 50, 60);
    ASSERT_EQ(e, LCD_ST7789_OK);
    /* Check CASET data: x0=10, x1=50 */
    ASSERT_EQ(data_log[0], 0);    /* x0 high */
    ASSERT_EQ(data_log[1], 10);   /* x0 low  */
    ASSERT_EQ(data_log[2], 0);    /* x1 high */
    ASSERT_EQ(data_log[3], 50);   /* x1 low  */
    /* Check RASET data: y0=20, y1=60 */
    ASSERT_EQ(data_log[4], 0);    /* y0 high */
    ASSERT_EQ(data_log[5], 20);   /* y0 low  */
    ASSERT_EQ(data_log[6], 0);    /* y1 high */
    ASSERT_EQ(data_log[7], 60);   /* y1 low  */
}

TEST(test_set_window_with_offset)
{
    mock_reset();
    lcd_st7789_cfg_t cfg_off = { .width = 240, .height = 240, .col_offset = 0, .row_offset = 80 };
    lcd_st7789_t d;
    lcd_st7789_init(&d, &mock_spi, &cfg_off);
    mock_reset();
    lcd_st7789_set_window(&d, 0, 0, 10, 10);
    /* RASET data should be offset by 80: y0=80, y1=90 */
    ASSERT_EQ(data_log[4], 0);
    ASSERT_EQ(data_log[5], 80);   /* 0 + 80 offset */
    ASSERT_EQ(data_log[6], 0);
    ASSERT_EQ(data_log[7], 90);   /* 10 + 80 offset */
}

TEST(test_set_window_null_dev)
{
    ASSERT_EQ(lcd_st7789_set_window(NULL, 0, 0, 10, 10),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_set_window_inverted_coords)
{
    setup();
    /* x0 > x1 */
    ASSERT_EQ(lcd_st7789_set_window(&dev, 50, 0, 10, 100),
              LCD_ST7789_ERR_PARAM);
    /* y0 > y1 */
    ASSERT_EQ(lcd_st7789_set_window(&dev, 0, 50, 100, 10),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_set_window_out_of_bounds)
{
    setup();
    /* x1 >= width */
    ASSERT_EQ(lcd_st7789_set_window(&dev, 0, 0, 240, 100),
              LCD_ST7789_ERR_PARAM);
    /* y1 >= height */
    ASSERT_EQ(lcd_st7789_set_window(&dev, 0, 0, 100, 320),
              LCD_ST7789_ERR_PARAM);
}

/* ========== write_pixels tests ========== */

TEST(test_write_pixels_ok)
{
    setup();
    lcd_st7789_set_window(&dev, 0, 0, 1, 0);
    mock_reset();
    uint8_t px[4] = { 0xF8, 0x00, 0x07, 0xE0 };  /* red + green in RGB565 */
    lcd_st7789_err_t e = lcd_st7789_write_pixels(&dev, px, 4);
    ASSERT_EQ(e, LCD_ST7789_OK);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_RAMWR) >= 0);
}

TEST(test_write_pixels_null_dev)
{
    uint8_t px[2] = { 0xFF, 0xFF };
    ASSERT_EQ(lcd_st7789_write_pixels(NULL, px, 2),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_write_pixels_null_data)
{
    setup();
    ASSERT_EQ(lcd_st7789_write_pixels(&dev, NULL, 2),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_write_pixels_zero_len)
{
    setup();
    uint8_t px[2] = { 0xFF, 0xFF };
    ASSERT_EQ(lcd_st7789_write_pixels(&dev, px, 0),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_write_pixels_odd_len)
{
    setup();
    uint8_t px[3] = { 0xFF, 0xFF, 0xFF };
    ASSERT_EQ(lcd_st7789_write_pixels(&dev, px, 3),
              LCD_ST7789_ERR_PARAM);
}

/* ========== fill_rect tests ========== */

TEST(test_fill_rect_ok)
{
    setup();
    mock_reset();
    lcd_st7789_err_t e = lcd_st7789_fill_rect(&dev, 0, 0, 2, 2, 0xF800);
    ASSERT_EQ(e, LCD_ST7789_OK);
    /* 2x2 = 4 pixels, each 2 bytes = 8 bytes of pixel data via xfer */
    /* Check that CASET + RASET + RAMWR were sent */
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_RASET) >= 0);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_RAMWR) >= 0);
}

TEST(test_fill_rect_pixel_data)
{
    setup();
    mock_reset();
    /* fill 1x1 with color 0xABCD */
    lcd_st7789_fill_rect(&dev, 10, 20, 1, 1, 0xABCD);
    /* data_log: first 8 bytes are CASET(4) + RASET(4), then 2 bytes pixel */
    ASSERT_EQ(data_log[8], 0xAB);   /* high byte */
    ASSERT_EQ(data_log[9], 0xCD);   /* low byte  */
}

TEST(test_fill_rect_null_dev)
{
    ASSERT_EQ(lcd_st7789_fill_rect(NULL, 0, 0, 10, 10, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_fill_rect_zero_size)
{
    setup();
    ASSERT_EQ(lcd_st7789_fill_rect(&dev, 0, 0, 0, 10, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
    ASSERT_EQ(lcd_st7789_fill_rect(&dev, 0, 0, 10, 0, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_fill_rect_out_of_bounds)
{
    setup();
    /* x + w > width */
    ASSERT_EQ(lcd_st7789_fill_rect(&dev, 230, 0, 20, 1, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
    /* y + h > height */
    ASSERT_EQ(lcd_st7789_fill_rect(&dev, 0, 310, 1, 20, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
}

/* ========== rotation tests ========== */

TEST(test_rotation_0)
{
    setup();
    lcd_st7789_err_t e = lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_0);
    ASSERT_EQ(e, LCD_ST7789_OK);
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
    ASSERT_EQ(dev.rotation, LCD_ST7789_ROT_0);
}

TEST(test_rotation_90)
{
    setup();
    lcd_st7789_err_t e = lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    ASSERT_EQ(e, LCD_ST7789_OK);
    ASSERT_EQ(dev.cur_width, 320);
    ASSERT_EQ(dev.cur_height, 240);
    ASSERT_EQ(dev.rotation, LCD_ST7789_ROT_90);
}

TEST(test_rotation_180)
{
    setup();
    lcd_st7789_err_t e = lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_180);
    ASSERT_EQ(e, LCD_ST7789_OK);
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
    ASSERT_EQ(dev.rotation, LCD_ST7789_ROT_180);
}

TEST(test_rotation_270)
{
    setup();
    lcd_st7789_err_t e = lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_270);
    ASSERT_EQ(e, LCD_ST7789_OK);
    ASSERT_EQ(dev.cur_width, 320);
    ASSERT_EQ(dev.cur_height, 240);
    ASSERT_EQ(dev.rotation, LCD_ST7789_ROT_270);
}

TEST(test_rotation_sends_madctl)
{
    setup();
    mock_reset();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_MADCTL) >= 0);
    /* MADCTL for ROT_90: MV | MY = 0x20 | 0x80 = 0xA0 */
    ASSERT_EQ(data_log[0], 0xA0);
}

TEST(test_rotation_invalid)
{
    setup();
    ASSERT_EQ(lcd_st7789_set_rotation(&dev, (lcd_st7789_rotation_t)4),
              LCD_ST7789_ERR_PARAM);
    ASSERT_EQ(lcd_st7789_set_rotation(&dev, (lcd_st7789_rotation_t)255),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_rotation_null_dev)
{
    ASSERT_EQ(lcd_st7789_set_rotation(NULL, LCD_ST7789_ROT_0),
              LCD_ST7789_ERR_PARAM);
}

TEST(test_rotation_90_offset_swap)
{
    /* Panel 240x240 with col_offset=0, row_offset=80 — rotation should swap offsets */
    mock_reset();
    lcd_st7789_cfg_t cfg_off = { .width = 240, .height = 240, .col_offset = 0, .row_offset = 80 };
    lcd_st7789_t d;
    lcd_st7789_init(&d, &mock_spi, &cfg_off);

    lcd_st7789_set_rotation(&d, LCD_ST7789_ROT_90);
    ASSERT_EQ(d.col_offset, 80);
    ASSERT_EQ(d.row_offset, 0);
}

/* ========== display on/off tests ========== */

TEST(test_display_on)
{
    setup();
    mock_reset();
    ASSERT_EQ(lcd_st7789_display_on(&dev), LCD_ST7789_OK);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_DISPON) >= 0);
}

TEST(test_display_off)
{
    setup();
    mock_reset();
    ASSERT_EQ(lcd_st7789_display_off(&dev), LCD_ST7789_OK);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_DISPOFF) >= 0);
}

TEST(test_display_on_null)
{
    ASSERT_EQ(lcd_st7789_display_on(NULL), LCD_ST7789_ERR_PARAM);
}

TEST(test_display_off_null)
{
    ASSERT_EQ(lcd_st7789_display_off(NULL), LCD_ST7789_ERR_PARAM);
}

/* ========== invert tests ========== */

TEST(test_invert_on)
{
    setup();
    mock_reset();
    ASSERT_EQ(lcd_st7789_invert(&dev, true), LCD_ST7789_OK);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_INVON) >= 0);
}

TEST(test_invert_off)
{
    setup();
    mock_reset();
    ASSERT_EQ(lcd_st7789_invert(&dev, false), LCD_ST7789_OK);
    ASSERT_TRUE(find_cmd(LCD_ST7789_CMD_INVOFF) >= 0);
}

TEST(test_invert_null)
{
    ASSERT_EQ(lcd_st7789_invert(NULL, true), LCD_ST7789_ERR_PARAM);
}

/* ========== error code tests ========== */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)LCD_ST7789_OK);
    ASSERT_EQ(-1, (int)LCD_ST7789_ERR_PARAM);
    ASSERT_EQ(-2, (int)LCD_ST7789_ERR_SPI);
}

/* ========== fill_rect after rotation ========== */

TEST(test_fill_rect_after_rotation)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    /* now width=320, height=240 — fill should work with swapped dims */
    lcd_st7789_err_t e = lcd_st7789_fill_rect(&dev, 0, 0, 320, 240, 0x001F);
    ASSERT_EQ(e, LCD_ST7789_OK);
}

TEST(test_fill_rect_after_rotation_oob)
{
    setup();
    lcd_st7789_set_rotation(&dev, LCD_ST7789_ROT_90);
    /* 321 width exceeds rotated cur_width (320) */
    ASSERT_EQ(lcd_st7789_fill_rect(&dev, 0, 0, 321, 1, 0xFFFF),
              LCD_ST7789_ERR_PARAM);
}

/* ========== runner ========== */

TEST(test_init_refuses_a_panel_past_the_frame_memory)
{
    lcd_st7789_cfg_t wide = { .width = 240, .height = 240, .col_offset = 1, .row_offset = 0 };
    lcd_st7789_cfg_t tall = { .width = 240, .height = 240, .col_offset = 0, .row_offset = 81 };
    lcd_st7789_cfg_t edge = { .width = 240, .height = 240, .col_offset = 0, .row_offset = 80 };
    lcd_st7789_t d;

    mock_reset();
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, &wide), LCD_ST7789_ERR_PARAM);
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, &tall), LCD_ST7789_ERR_PARAM);
    ASSERT_EQ(lcd_st7789_init(&d, &mock_spi, &edge), LCD_ST7789_OK);
}

TEST_MAIN("lcd_st7789")
    /* init */
    RUN(test_init_ok);
    RUN(test_init_sends_reset_sequence);
    RUN(test_init_sends_swreset);
    RUN(test_init_sends_slpout);
    RUN(test_init_sends_colmod);
    RUN(test_init_sends_madctl);
    RUN(test_init_sends_invon);
    RUN(test_init_sends_dispon);
    RUN(test_init_sets_dimensions);
    /* null / param safety */
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_null_cfg);
    RUN(test_init_zero_width);
    RUN(test_init_zero_height);
    RUN(test_init_missing_hal_func);
    RUN(test_init_missing_rst_func);
    RUN(test_init_missing_delay_func);
    /* set_window */
    RUN(test_set_window_ok);
    RUN(test_set_window_small_region);
    RUN(test_set_window_with_offset);
    RUN(test_set_window_null_dev);
    RUN(test_set_window_inverted_coords);
    RUN(test_set_window_out_of_bounds);
    /* write_pixels */
    RUN(test_write_pixels_ok);
    RUN(test_write_pixels_null_dev);
    RUN(test_write_pixels_null_data);
    RUN(test_write_pixels_zero_len);
    RUN(test_write_pixels_odd_len);
    /* fill_rect */
    RUN(test_fill_rect_ok);
    RUN(test_fill_rect_pixel_data);
    RUN(test_fill_rect_null_dev);
    RUN(test_fill_rect_zero_size);
    RUN(test_fill_rect_out_of_bounds);
    /* rotation */
    RUN(test_rotation_0);
    RUN(test_rotation_90);
    RUN(test_rotation_180);
    RUN(test_rotation_270);
    RUN(test_rotation_sends_madctl);
    RUN(test_rotation_invalid);
    RUN(test_rotation_null_dev);
    RUN(test_rotation_90_offset_swap);
    /* display on/off */
    RUN(test_display_on);
    RUN(test_display_off);
    RUN(test_display_on_null);
    RUN(test_display_off_null);
    /* invert */
    RUN(test_invert_on);
    RUN(test_invert_off);
    RUN(test_invert_null);
    /* error codes */
    RUN(test_error_codes);
    /* rotation + fill_rect integration */
    RUN(test_fill_rect_after_rotation);
    RUN(test_fill_rect_after_rotation_oob);
    RUN(test_init_refuses_a_panel_past_the_frame_memory);
TEST_MAIN_END()
