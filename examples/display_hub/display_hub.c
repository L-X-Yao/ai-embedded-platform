// SPDX-License-Identifier: MIT
/*
 * display_hub -- integration recipe: driving multiple display types
 *
 * Demonstrates six L1 display wheels working from a single application:
 *   L1  sh1106      -- 128x64 OLED (I2C, framebuffer)
 *   L1  hd44780     -- 16x2 character LCD (I2C via PCF8574)
 *   L1  tm1637      -- 4-digit 7-segment LED (GPIO bit-bang)
 *   L1  max7219     -- 8x8 LED matrix / 7-segment (SPI)
 *   L1  lcd_st7789  -- 240x240 color TFT (SPI, RGB565)
 *   L1  ili9341     -- 320x240 color TFT (SPI, RGB565)
 *
 * Runs entirely on host with mock I2C / GPIO / SPI -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/display_hub
 */

#include "test_harness.h"

/* wheels */
#include "sh1106.h"
#include "hd44780.h"
#include "tm1637.h"
#include "max7219.h"
#include "lcd_st7789.h"
#include "ili9341.h"

#include <string.h>

/* ======================================================================
 * Mock I2C -- shared by SH1106 and HD44780
 *
 * Both use write_reg / read_regs interface. We track operation counts
 * per device by address.
 * ====================================================================== */

static int i2c_op_count;

static bool mock_i2c_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                               uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)val;
    i2c_op_count++;
    return true;
}

static bool mock_i2c_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                               uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    /* Return zeros for any read */
    memset(buf, 0, len);
    i2c_op_count++;
    return true;
}

static void mock_i2c_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* SH1106 I2C HAL */
static const sh1106_i2c_t sh1106_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* HD44780 I2C HAL (reuses same mock, delay_ms is required) */
static const hd44780_i2c_t hd44780_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock GPIO -- TM1637 bit-bang interface
 *
 * Track CLK/DIO state transitions for verification.
 * ====================================================================== */

static int  gpio_op_count;
static bool gpio_dio_state;  /* simulated DIO line level */

static void mock_clk_low(void *ctx)  { (void)ctx; gpio_op_count++; }
static void mock_clk_high(void *ctx) { (void)ctx; gpio_op_count++; }
static void mock_dio_low(void *ctx)  { (void)ctx; gpio_op_count++; gpio_dio_state = false; }
static void mock_dio_high(void *ctx) { (void)ctx; gpio_op_count++; gpio_dio_state = true; }

static bool mock_dio_read(void *ctx)
{
    (void)ctx;
    gpio_op_count++;
    return false;  /* ACK: DIO pulled low by slave */
}

static void mock_delay_us(void *ctx, uint16_t us)
{
    (void)ctx; (void)us;
}

static const tm1637_gpio_t tm1637_gpio = {
    .clk_low   = mock_clk_low,
    .clk_high  = mock_clk_high,
    .dio_low   = mock_dio_low,
    .dio_high  = mock_dio_high,
    .dio_read  = mock_dio_read,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock SPI -- shared by MAX7219
 *
 * MAX7219 uses transfer(ctx, tx, rx, len) + cs_low/cs_high.
 * Track transferred bytes for verification.
 * ====================================================================== */

#define SPI_LOG_MAX 256
static uint8_t max_spi_log[SPI_LOG_MAX];
static int     max_spi_log_pos;
static int     max_spi_op_count;

static bool mock_max_spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                                  uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        if (max_spi_log_pos < SPI_LOG_MAX) {
            max_spi_log[max_spi_log_pos++] = tx[i];
        }
        if (rx) {
            rx[i] = 0;
        }
    }
    max_spi_op_count++;
    return true;
}

static void mock_max_cs_low(void *ctx)  { (void)ctx; }
static void mock_max_cs_high(void *ctx) { (void)ctx; }

static const max7219_spi_t max7219_spi = {
    .transfer = mock_max_spi_transfer,
    .cs_low   = mock_max_cs_low,
    .cs_high  = mock_max_cs_high,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock SPI -- shared by LCD_ST7789 and ILI9341
 *
 * These use a different SPI HAL: xfer(ctx, byte), dc_set, rst_set.
 * We track command/data bytes separately.
 * ====================================================================== */

#define TFT_LOG_MAX 512
static uint8_t tft_cmd_log[TFT_LOG_MAX];
static int     tft_cmd_log_pos;
static uint8_t tft_data_log[TFT_LOG_MAX];
static int     tft_data_log_pos;
static bool    tft_dc_level;    /* false=cmd, true=data */
static int     tft_xfer_count;

static void mock_tft_cs_low(void *ctx)  { (void)ctx; }
static void mock_tft_cs_high(void *ctx) { (void)ctx; }

static uint8_t mock_tft_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    tft_xfer_count++;
    if (tft_dc_level) {
        /* data mode */
        if (tft_data_log_pos < TFT_LOG_MAX) {
            tft_data_log[tft_data_log_pos++] = tx;
        }
    } else {
        /* command mode */
        if (tft_cmd_log_pos < TFT_LOG_MAX) {
            tft_cmd_log[tft_cmd_log_pos++] = tx;
        }
    }
    return 0;
}

static void mock_tft_dc_set(void *ctx, bool level)
{
    (void)ctx;
    tft_dc_level = level;
}

static void mock_tft_rst_set(void *ctx, bool level)
{
    (void)ctx; (void)level;
}

static void mock_tft_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* ST7789 SPI HAL */
static const lcd_st7789_spi_t st7789_spi = {
    .cs_low   = mock_tft_cs_low,
    .cs_high  = mock_tft_cs_high,
    .xfer     = mock_tft_xfer,
    .dc_set   = mock_tft_dc_set,
    .rst_set  = mock_tft_rst_set,
    .delay_ms = mock_tft_delay_ms,
    .ctx      = NULL,
};

/* ILI9341 SPI HAL (reuses same mock -- separate device context tracks state) */
static const ili9341_spi_t ili9341_spi = {
    .cs_low   = mock_tft_cs_low,
    .cs_high  = mock_tft_cs_high,
    .xfer     = mock_tft_xfer,
    .dc_set   = mock_tft_dc_set,
    .rst_set  = mock_tft_rst_set,
    .delay_ms = mock_tft_delay_ms,
    .ctx      = NULL,
};

/* ======================================================================
 * Global device instances
 * ====================================================================== */

static sh1106_t      oled_dev;
static uint8_t       oled_fb[SH1106_BUF_SIZE];

static hd44780_t     lcd_dev;

static tm1637_t      seg_dev;

static max7219_t     matrix_dev;

static lcd_st7789_t  st7789_dev;

static ili9341_t     ili9341_dev;

/* ======================================================================
 * Helper -- reset all mock counters
 * ====================================================================== */

static void reset_all_mocks(void)
{
    i2c_op_count = 0;

    gpio_op_count = 0;
    gpio_dio_state = true;

    max_spi_log_pos = 0;
    max_spi_op_count = 0;
    memset(max_spi_log, 0, sizeof(max_spi_log));

    tft_cmd_log_pos = 0;
    tft_data_log_pos = 0;
    tft_dc_level = false;
    tft_xfer_count = 0;
    memset(tft_cmd_log, 0, sizeof(tft_cmd_log));
    memset(tft_data_log, 0, sizeof(tft_data_log));
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    reset_all_mocks();

    /* 1. SH1106 OLED (I2C, 128x64) */
    sh1106_err_t sh_err = sh1106_init(&oled_dev, &sh1106_i2c,
                                      SH1106_ADDR_A, oled_fb);
    ASSERT_EQ(sh_err, SH1106_OK);
    ASSERT_TRUE(i2c_op_count > 0);  /* init sends many commands */

    int i2c_after_sh = i2c_op_count;

    /* 2. HD44780 character LCD (I2C via PCF8574, 16x2) */
    hd44780_err_t hd_err = hd44780_init(&lcd_dev, &hd44780_i2c,
                                        HD44780_ADDR_PCF8574, 16, 2);
    ASSERT_EQ(hd_err, HD44780_OK);
    ASSERT_TRUE(i2c_op_count > i2c_after_sh);  /* HD44780 init uses I2C */

    /* 3. TM1637 7-segment (GPIO bit-bang) */
    tm1637_err_t tm_err = tm1637_init(&seg_dev, &tm1637_gpio);
    ASSERT_EQ(tm_err, TM1637_OK);
    ASSERT_TRUE(gpio_op_count > 0);  /* init clears display via bit-bang */

    /* 4. MAX7219 LED matrix (SPI) */
    max7219_err_t mx_err = max7219_init(&matrix_dev, &max7219_spi);
    ASSERT_EQ(mx_err, MAX7219_OK);
    ASSERT_TRUE(max_spi_op_count > 0);  /* init configures registers */

    /* 5. LCD_ST7789 color TFT (SPI, 240x240) */
    lcd_st7789_cfg_t st_cfg = {
        .width      = 240,
        .height     = 240,
        .col_offset = 0,
        .row_offset = 0,
    };
    lcd_st7789_err_t st_err = lcd_st7789_init(&st7789_dev, &st7789_spi,
                                              &st_cfg);
    ASSERT_EQ(st_err, LCD_ST7789_OK);
    ASSERT_TRUE(tft_xfer_count > 0);  /* init sends commands */

    /* 6. ILI9341 color TFT (SPI, 320x240) */
    int xfer_before_ili = tft_xfer_count;
    ili9341_err_t ili_err = ili9341_init(&ili9341_dev, &ili9341_spi);
    ASSERT_EQ(ili_err, ILI9341_OK);
    ASSERT_TRUE(tft_xfer_count > xfer_before_ili);

    printf("  All 6 displays initialized successfully\n");
    printf("  I2C ops: %d, GPIO ops: %d, MAX SPI ops: %d, TFT xfers: %d\n",
           i2c_op_count, gpio_op_count, max_spi_op_count, tft_xfer_count);
}

TEST(test_sh1106_draw_pixel)
{
    /* Clear framebuffer, draw a few pixels, verify via get_pixel */
    sh1106_clear(&oled_dev);

    /* Verify buffer is clear */
    ASSERT_FALSE(sh1106_get_pixel(&oled_dev, 0, 0));
    ASSERT_FALSE(sh1106_get_pixel(&oled_dev, 63, 31));

    /* Draw a small cross pattern at center (64, 32) */
    sh1106_set_pixel(&oled_dev, 64, 32, true);
    sh1106_set_pixel(&oled_dev, 63, 32, true);
    sh1106_set_pixel(&oled_dev, 65, 32, true);
    sh1106_set_pixel(&oled_dev, 64, 31, true);
    sh1106_set_pixel(&oled_dev, 64, 33, true);

    /* Verify pixels are set */
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 64, 32));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 63, 32));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 65, 32));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 64, 31));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 64, 33));

    /* Verify adjacent pixels are still off */
    ASSERT_FALSE(sh1106_get_pixel(&oled_dev, 62, 32));
    ASSERT_FALSE(sh1106_get_pixel(&oled_dev, 66, 32));

    /* Flush to display (all I2C ops succeed via mock) */
    sh1106_err_t err = sh1106_display(&oled_dev);
    ASSERT_EQ(err, SH1106_OK);

    /* Turn a pixel off and verify */
    sh1106_set_pixel(&oled_dev, 64, 32, false);
    ASSERT_FALSE(sh1106_get_pixel(&oled_dev, 64, 32));

    printf("  SH1106: cross pattern drawn and verified in framebuffer\n");
}

TEST(test_hd44780_write_text)
{
    reset_all_mocks();

    /* Set cursor to beginning of line 0 */
    hd44780_err_t err = hd44780_set_cursor(&lcd_dev, 0, 0);
    ASSERT_EQ(err, HD44780_OK);

    int ops_before = i2c_op_count;

    /* Write a string -- each character sends I2C data */
    err = hd44780_write_str(&lcd_dev, "Hello Display!");
    ASSERT_EQ(err, HD44780_OK);

    /* Verify I2C was exercised (each char = 2 nibbles, each nibble =
     * 2 expander writes for EN pulse + 1 data write = multiple ops) */
    ASSERT_TRUE(i2c_op_count > ops_before);

    /* Move to line 2 and write */
    err = hd44780_set_cursor(&lcd_dev, 0, 1);
    ASSERT_EQ(err, HD44780_OK);

    err = hd44780_write_str(&lcd_dev, "Line 2 text");
    ASSERT_EQ(err, HD44780_OK);

    /* Test backlight control */
    err = hd44780_backlight(&lcd_dev, false);
    ASSERT_EQ(err, HD44780_OK);
    err = hd44780_backlight(&lcd_dev, true);
    ASSERT_EQ(err, HD44780_OK);

    printf("  HD44780: wrote text to both lines, %d I2C ops\n",
           i2c_op_count);
}

TEST(test_tm1637_display_number)
{
    reset_all_mocks();

    /* Display the number 1234 */
    tm1637_err_t err = tm1637_display_number(&seg_dev, 1234, false);
    ASSERT_EQ(err, TM1637_OK);
    ASSERT_TRUE(gpio_op_count > 0);

    /* Verify shadow buffer has the expected segment patterns */
    ASSERT_EQ(seg_dev.segments[0], tm1637_encode_digit(1));
    ASSERT_EQ(seg_dev.segments[1], tm1637_encode_digit(2));
    ASSERT_EQ(seg_dev.segments[2], tm1637_encode_digit(3));
    ASSERT_EQ(seg_dev.segments[3], tm1637_encode_digit(4));

    /* Display with leading zeros */
    int ops_before = gpio_op_count;
    err = tm1637_display_number(&seg_dev, 42, true);
    ASSERT_EQ(err, TM1637_OK);
    ASSERT_TRUE(gpio_op_count > ops_before);

    /* Set colon (useful for clock display) */
    err = tm1637_set_colon(&seg_dev, true);
    ASSERT_EQ(err, TM1637_OK);
    ASSERT_TRUE((seg_dev.segments[1] & 0x80) != 0);

    /* Clear colon */
    err = tm1637_set_colon(&seg_dev, false);
    ASSERT_EQ(err, TM1637_OK);
    ASSERT_TRUE((seg_dev.segments[1] & 0x80) == 0);

    printf("  TM1637: displayed numbers, %d GPIO ops\n", gpio_op_count);
}

TEST(test_max7219_set_matrix)
{
    reset_all_mocks();

    /* Set an X pattern on the 8x8 matrix:
     * row 0: 0x81 (corners), row 1: 0x42, row 2: 0x24, row 3: 0x18,
     * row 4: 0x18, row 5: 0x24, row 6: 0x42, row 7: 0x81
     */
    static const uint8_t x_pattern[8] = {
        0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81,
    };

    for (uint8_t row = 0; row < 8; row++) {
        max7219_err_t err = max7219_set_digit(&matrix_dev, row,
                                              x_pattern[row]);
        ASSERT_EQ(err, MAX7219_OK);
    }

    /* Verify SPI log contains the digit register writes.
     * Each set_digit sends 2 bytes: [reg, val].
     * Digit registers are 0x01-0x08. */
    ASSERT_TRUE(max_spi_log_pos >= 16);

    /* Check first row: register 0x01, value 0x81 */
    ASSERT_EQ(max_spi_log[0], 0x01);  /* REG_DIGIT0 + 0 = 0x01 */
    ASSERT_EQ(max_spi_log[1], 0x81);

    /* Check last row: register 0x08, value 0x81 */
    ASSERT_EQ(max_spi_log[14], 0x08);  /* REG_DIGIT0 + 7 = 0x08 */
    ASSERT_EQ(max_spi_log[15], 0x81);

    /* Set brightness */
    max7219_err_t err = max7219_set_intensity(&matrix_dev, 8);
    ASSERT_EQ(err, MAX7219_OK);

    printf("  MAX7219: X pattern written, %d SPI transfers\n",
           max_spi_op_count);
}

TEST(test_st7789_fill_color)
{
    reset_all_mocks();

    /* Fill a small 4x4 region with red (RGB565: 0xF800) */
    lcd_st7789_err_t err = lcd_st7789_fill_rect(&st7789_dev,
                                                0, 0, 4, 4, 0xF800);
    ASSERT_EQ(err, LCD_ST7789_OK);

    /* Verify command log contains CASET (0x2A), RASET (0x2B), RAMWR (0x2C) */
    bool found_caset = false;
    bool found_raset = false;
    bool found_ramwr = false;
    for (int i = 0; i < tft_cmd_log_pos; i++) {
        if (tft_cmd_log[i] == LCD_ST7789_CMD_CASET) found_caset = true;
        if (tft_cmd_log[i] == LCD_ST7789_CMD_RASET) found_raset = true;
        if (tft_cmd_log[i] == LCD_ST7789_CMD_RAMWR) found_ramwr = true;
    }
    ASSERT_TRUE(found_caset);
    ASSERT_TRUE(found_raset);
    ASSERT_TRUE(found_ramwr);

    /* Verify pixel data: 4x4 = 16 pixels, 2 bytes each = 32 data bytes.
     * The data log includes window setup bytes (8 bytes for CASET+RASET)
     * plus 32 pixel bytes = 40 total data bytes minimum. */
    ASSERT_TRUE(tft_data_log_pos >= 40);

    /* Verify the pixel data is the correct color.
     * Window setup sends 8 bytes (4 for CASET + 4 for RASET),
     * then 32 bytes of pixel data follow. */
    uint8_t color_hi = (uint8_t)(0xF800 >> 8);
    uint8_t color_lo = (uint8_t)(0xF800 & 0xFF);
    /* Check first pixel in the fill data (after 8 bytes of window setup) */
    ASSERT_EQ(tft_data_log[8], color_hi);
    ASSERT_EQ(tft_data_log[9], color_lo);

    printf("  ST7789: filled 4x4 region with red, %d xfers\n",
           tft_xfer_count);
}

TEST(test_ili9341_set_window)
{
    reset_all_mocks();

    /* Set a drawing window */
    ili9341_err_t err = ili9341_set_window(&ili9341_dev, 10, 20, 50, 60);
    ASSERT_EQ(err, ILI9341_OK);

    /* Verify CASET and RASET commands were sent */
    bool found_caset = false;
    bool found_raset = false;
    for (int i = 0; i < tft_cmd_log_pos; i++) {
        if (tft_cmd_log[i] == ILI9341_CMD_CASET) found_caset = true;
        if (tft_cmd_log[i] == ILI9341_CMD_RASET) found_raset = true;
    }
    ASSERT_TRUE(found_caset);
    ASSERT_TRUE(found_raset);

    /* Verify CASET data: x0=10, x1=50 -> [0x00, 0x0A, 0x00, 0x32] */
    ASSERT_EQ(tft_data_log[0], 0x00);
    ASSERT_EQ(tft_data_log[1], 10);
    ASSERT_EQ(tft_data_log[2], 0x00);
    ASSERT_EQ(tft_data_log[3], 50);

    /* Verify RASET data: y0=20, y1=60 -> [0x00, 0x14, 0x00, 0x3C] */
    ASSERT_EQ(tft_data_log[4], 0x00);
    ASSERT_EQ(tft_data_log[5], 20);
    ASSERT_EQ(tft_data_log[6], 0x00);
    ASSERT_EQ(tft_data_log[7], 60);

    /* Fill with blue (RGB565: 0x001F) using fill_rect */
    reset_all_mocks();
    uint16_t blue = ili9341_rgb565(0, 0, 255);
    /* rgb565(0,0,255) = 0x001F */
    ASSERT_EQ(blue, 0x001F);

    err = ili9341_fill_rect(&ili9341_dev, 0, 0, 2, 2, blue);
    ASSERT_EQ(err, ILI9341_OK);

    /* 2x2 = 4 pixels * 2 bytes = 8 pixel bytes after window setup */
    ASSERT_TRUE(tft_data_log_pos >= 16);  /* 8 window + 8 pixel */

    printf("  ILI9341: window set and filled with blue, %d xfers\n",
           tft_xfer_count);
}

TEST(test_full_display_cycle)
{
    reset_all_mocks();

    printf("\n  --- Full Display Cycle ---\n");

    /* SH1106: draw diagonal line */
    sh1106_clear(&oled_dev);
    for (uint8_t i = 0; i < 64; i++) {
        sh1106_set_pixel(&oled_dev, (uint8_t)(i * 2), i, true);
    }
    sh1106_err_t sh_err = sh1106_display(&oled_dev);
    ASSERT_EQ(sh_err, SH1106_OK);
    /* Verify some pixels on the diagonal */
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 0, 0));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 20, 10));
    ASSERT_TRUE(sh1106_get_pixel(&oled_dev, 126, 63));
    printf("  SH1106: diagonal line drawn and flushed\n");

    /* HD44780: clear and write */
    hd44780_err_t hd_err = hd44780_clear(&lcd_dev);
    ASSERT_EQ(hd_err, HD44780_OK);
    hd_err = hd44780_set_cursor(&lcd_dev, 0, 0);
    ASSERT_EQ(hd_err, HD44780_OK);
    hd_err = hd44780_write_str(&lcd_dev, "DisplayHub");
    ASSERT_EQ(hd_err, HD44780_OK);
    printf("  HD44780: cleared and wrote 'DisplayHub'\n");

    /* TM1637: display time-like 12:34 */
    tm1637_err_t tm_err = tm1637_display_number(&seg_dev, 1234, true);
    ASSERT_EQ(tm_err, TM1637_OK);
    tm_err = tm1637_set_colon(&seg_dev, true);
    ASSERT_EQ(tm_err, TM1637_OK);
    ASSERT_TRUE((seg_dev.segments[1] & 0x80) != 0);
    printf("  TM1637: displayed 12:34 with colon\n");

    /* MAX7219: checkerboard pattern */
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t pattern = (row % 2 == 0) ? 0xAA : 0x55;
        max7219_err_t mx_err = max7219_set_digit(&matrix_dev, row, pattern);
        ASSERT_EQ(mx_err, MAX7219_OK);
    }
    printf("  MAX7219: checkerboard pattern set\n");

    /* ST7789: fill small region with green */
    uint16_t green = 0x07E0;
    lcd_st7789_err_t st_err = lcd_st7789_fill_rect(&st7789_dev,
                                                   10, 10, 8, 8, green);
    ASSERT_EQ(st_err, LCD_ST7789_OK);
    printf("  ST7789: green 8x8 square at (10,10)\n");

    /* ILI9341: fill small region with white */
    uint16_t white = ili9341_rgb565(255, 255, 255);
    ili9341_err_t ili_err = ili9341_fill_rect(&ili9341_dev,
                                             0, 0, 4, 4, white);
    ASSERT_EQ(ili_err, ILI9341_OK);
    printf("  ILI9341: white 4x4 square at (0,0)\n");

    printf("  --- Full cycle complete: 6 displays driven ---\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("display_hub integration")
    RUN(test_init_all);
    RUN(test_sh1106_draw_pixel);
    RUN(test_hd44780_write_text);
    RUN(test_tm1637_display_number);
    RUN(test_max7219_set_matrix);
    RUN(test_st7789_fill_color);
    RUN(test_ili9341_set_window);
    RUN(test_full_display_cycle);
TEST_MAIN_END()
