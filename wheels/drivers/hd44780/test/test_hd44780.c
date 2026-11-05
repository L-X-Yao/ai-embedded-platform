// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "hd44780.h"

/* ---------- mock I2C for PCF8574 ---------- */

/* PCF8574 is a simple GPIO expander: writes set pin state, reads return
 * pin state. For the HD44780 backpack, write_reg is used to set all pins.
 * We track the byte sequence sent. */

static int     mock_fail_at;
static int     mock_op_count;
static int     mock_write_count;
static uint32_t mock_delay_count;

/* Log of bytes written to expander */
static uint8_t write_log[512];
static int     write_log_idx;

static void mock_reset(void)
{
    mock_fail_at    = -1;
    mock_op_count   = 0;
    mock_write_count = 0;
    mock_delay_count = 0;
    write_log_idx   = 0;
    memset(write_log, 0, sizeof(write_log));
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_reg(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)val;
    if (mock_should_fail()) return false;
    mock_write_count++;
    if (write_log_idx < (int)sizeof(write_log))
        write_log[write_log_idx++] = reg;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    memset(buf, 0, len);
    return true;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    mock_delay_count++;
}

static const hd44780_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .delay_ms  = mock_delay_ms,
    .ctx       = NULL,
};

static hd44780_t dev;

static void setup_16x2(void)
{
    mock_reset();
    hd44780_init(&dev, &mock_i2c, HD44780_ADDR_PCF8574, 16, 2);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    hd44780_err_t e = hd44780_init(&dev, &mock_i2c, HD44780_ADDR_PCF8574, 16, 2);
    ASSERT_EQ(e, HD44780_OK);
    ASSERT_TRUE(mock_write_count > 10);
    ASSERT_TRUE(mock_delay_count > 0);
}

TEST(test_init_20x4)
{
    mock_reset();
    hd44780_err_t e = hd44780_init(&dev, &mock_i2c, HD44780_ADDR_PCF8574A, 20, 4);
    ASSERT_EQ(e, HD44780_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    hd44780_err_t e = hd44780_init(&dev, &mock_i2c, HD44780_ADDR_PCF8574, 16, 2);
    ASSERT_EQ(e, HD44780_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(hd44780_init(NULL, &mock_i2c, 0x27, 16, 2), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_init(&dev, NULL, 0x27, 16, 2), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_clear(NULL), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_set_cursor(NULL, 0, 0), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_write_char(NULL, 'A'), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_write_str(NULL, "hi"), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_backlight(NULL, true), HD44780_ERR_PARAM);
}

TEST(test_null_str)
{
    setup_16x2();
    ASSERT_EQ(hd44780_write_str(&dev, NULL), HD44780_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    hd44780_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    hd44780_t zero_dev;
    ASSERT_TRUE(hd44780_init(&zero_dev, &zero_hal, 0x27, 16, 2) < 0);
}

TEST(test_invalid_dimensions)
{
    mock_reset();
    ASSERT_EQ(hd44780_init(&dev, &mock_i2c, 0x27, 0, 2), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_init(&dev, &mock_i2c, 0x27, 16, 0), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_init(&dev, &mock_i2c, 0x27, 41, 2), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_init(&dev, &mock_i2c, 0x27, 16, 5), HD44780_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)HD44780_ERR_I2C);
    ASSERT_EQ(-2, (int)HD44780_ERR_ID);
    ASSERT_EQ(-3, (int)HD44780_ERR_PARAM);
}

TEST(test_clear)
{
    setup_16x2();
    mock_reset();
    hd44780_err_t e = hd44780_clear(&dev);
    ASSERT_EQ(e, HD44780_OK);
    ASSERT_TRUE(mock_write_count > 0);
    ASSERT_TRUE(mock_delay_count > 0);
}

TEST(test_clear_i2c_fail)
{
    setup_16x2();
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(hd44780_clear(&dev), HD44780_ERR_I2C);
}

TEST(test_set_cursor_valid)
{
    setup_16x2();
    mock_reset();
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 0), HD44780_OK);
    ASSERT_EQ(hd44780_set_cursor(&dev, 15, 1), HD44780_OK);
}

TEST(test_set_cursor_out_of_range)
{
    setup_16x2();
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 2), HD44780_ERR_PARAM);
    ASSERT_EQ(hd44780_set_cursor(&dev, 16, 0), HD44780_ERR_PARAM);
}

TEST(test_write_char)
{
    setup_16x2();
    mock_reset();
    hd44780_err_t e = hd44780_write_char(&dev, 'A');
    ASSERT_EQ(e, HD44780_OK);
    ASSERT_TRUE(mock_write_count > 0);
}

TEST(test_write_char_i2c_fail)
{
    setup_16x2();
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(hd44780_write_char(&dev, 'X'), HD44780_ERR_I2C);
}

TEST(test_write_str)
{
    setup_16x2();
    mock_reset();
    hd44780_err_t e = hd44780_write_str(&dev, "Hello");
    ASSERT_EQ(e, HD44780_OK);
    /* 5 chars, each requires 4 nibble writes (2 nibbles * 2 for EN toggle) */
    ASSERT_TRUE(mock_write_count >= 20);
}

TEST(test_write_str_empty)
{
    setup_16x2();
    mock_reset();
    ASSERT_EQ(hd44780_write_str(&dev, ""), HD44780_OK);
    ASSERT_EQ(mock_write_count, 0);
}

TEST(test_write_str_i2c_fail)
{
    setup_16x2();
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(hd44780_write_str(&dev, "Hi"), HD44780_ERR_I2C);
}

TEST(test_backlight_on_off)
{
    setup_16x2();
    mock_reset();
    ASSERT_EQ(hd44780_backlight(&dev, false), HD44780_OK);
    ASSERT_TRUE(mock_write_count > 0);
    /* Verify backlight bit is 0 in last write */
    ASSERT_EQ(write_log[write_log_idx - 1] & 0x08, 0);

    mock_reset();
    ASSERT_EQ(hd44780_backlight(&dev, true), HD44780_OK);
    /* Verify backlight bit is set */
    ASSERT_EQ(write_log[write_log_idx - 1] & 0x08, 0x08);
}

TEST(test_backlight_i2c_fail)
{
    setup_16x2();
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(hd44780_backlight(&dev, true), HD44780_ERR_I2C);
}

TEST(test_write_char_sends_rs_high)
{
    /* Data writes should have RS bit (bit 0) set */
    setup_16x2();
    mock_reset();
    hd44780_write_char(&dev, 'A');
    /* Check that at least some write bytes have RS=1 */
    int rs_found = 0;
    for (int i = 0; i < write_log_idx; i++) {
        if (write_log[i] & 0x01) rs_found++;
    }
    ASSERT_TRUE(rs_found > 0);
}

TEST(test_4x_row_addressing)
{
    /* 20x4 LCD should accept rows 0-3 */
    mock_reset();
    hd44780_init(&dev, &mock_i2c, 0x27, 20, 4);
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 0), HD44780_OK);
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 1), HD44780_OK);
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 2), HD44780_OK);
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 3), HD44780_OK);
    ASSERT_EQ(hd44780_set_cursor(&dev, 0, 4), HD44780_ERR_PARAM);
}

/* Commands sent, rebuilt from the EN-high writes: two nibbles each, after the four reset nibbles. */
static int sent_commands(uint8_t *out, int cap)
{
    uint8_t nibbles[64];
    int n = 0;
    for (int i = 0; i < write_log_idx && n < 64; i++) {
        if (write_log[i] & 0x04u) nibbles[n++] = (uint8_t)(write_log[i] >> 4);
    }
    int count = 0;
    for (int i = 4; i + 1 < n && count < cap; i += 2) {
        out[count++] = (uint8_t)(nibbles[i] << 4 | nibbles[i + 1]);
    }
    return count;
}

TEST(test_init_switches_the_display_on_last)
{
    uint8_t cmds[8];
    mock_reset();
    ASSERT_EQ(hd44780_init(&dev, &mock_i2c, HD44780_ADDR_PCF8574, 16, 2), HD44780_OK);

    ASSERT_EQ(sent_commands(cmds, 8), 5);
    ASSERT_EQ_HEX(cmds[0], 0x28);  /* function set */
    ASSERT_EQ_HEX(cmds[1], 0x08);  /* display off */
    ASSERT_EQ_HEX(cmds[2], 0x01);  /* clear */
    ASSERT_EQ_HEX(cmds[3], 0x06);  /* entry mode */
    ASSERT_EQ_HEX(cmds[4], 0x0C);  /* display on */
}

TEST_MAIN("hd44780")
    RUN(test_init_ok);
    RUN(test_init_20x4);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_null_str);
    RUN(test_zero_init_safety);
    RUN(test_invalid_dimensions);
    RUN(test_error_codes);
    RUN(test_clear);
    RUN(test_clear_i2c_fail);
    RUN(test_set_cursor_valid);
    RUN(test_set_cursor_out_of_range);
    RUN(test_write_char);
    RUN(test_write_char_i2c_fail);
    RUN(test_write_str);
    RUN(test_write_str_empty);
    RUN(test_write_str_i2c_fail);
    RUN(test_backlight_on_off);
    RUN(test_backlight_i2c_fail);
    RUN(test_write_char_sends_rs_high);
    RUN(test_4x_row_addressing);
    RUN(test_init_switches_the_display_on_last);
TEST_MAIN_END()
