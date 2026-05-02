// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "oled_ssd1306.h"

static int mock_cmd_count;
static int mock_data_count;
static int mock_fail_next;
static int mock_op_count;
static int mock_fail_at;
static uint8_t last_cmd;
static uint8_t cmd_log[64];
static int cmd_log_count;
static const uint8_t *last_data_ptr;
static uint16_t last_data_len;

static void mock_reset(void)
{
    mock_cmd_count = 0;
    mock_data_count = 0;
    mock_fail_next = 0;
    mock_op_count = 0;
    mock_fail_at = -1;
    last_cmd = 0;
    cmd_log_count = 0;
    memset(cmd_log, 0, sizeof(cmd_log));
    last_data_ptr = NULL;
    last_data_len = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_cmd(void *ctx, uint8_t addr, uint8_t c)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    if (cmd_log_count < 64) cmd_log[cmd_log_count++] = c;
    mock_cmd_count++;
    last_cmd = c;
    return true;
}

static bool mock_write_data(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_data_count++;
    last_data_ptr = data;
    last_data_len = len;
    return true;
}

static const oled_ssd1306_i2c_t mock_i2c = {
    .write_cmd = mock_write_cmd,
    .write_data = mock_write_data,
    .ctx = NULL,
};
static oled_ssd1306_t dev;
static uint8_t framebuf[OLED_SSD1306_BUF_SIZE(128, 64)];
static oled_ssd1306_cfg_t default_cfg = {
    .addr = OLED_SSD1306_ADDR_A,
    .width = 128,
    .height = 64,
};

static void setup(void)
{
    mock_reset();
    oled_ssd1306_init(&dev, &mock_i2c, &default_cfg, framebuf);
}

TEST(test_init_ok)
{
    mock_reset();
    oled_ssd1306_t d;
    oled_ssd1306_err_t e = oled_ssd1306_init(&d, &mock_i2c, &default_cfg, framebuf);
    ASSERT_EQ(e, OLED_SSD1306_OK);
    ASSERT_TRUE(mock_cmd_count > 10);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    oled_ssd1306_t d;
    oled_ssd1306_err_t e = oled_ssd1306_init(&d, &mock_i2c, &default_cfg, framebuf);
    ASSERT_EQ(e, OLED_SSD1306_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(oled_ssd1306_init(NULL, &mock_i2c, &default_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM);
    oled_ssd1306_t d;
    ASSERT_EQ(oled_ssd1306_init(&d, NULL, &default_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM);
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, NULL, framebuf),
              OLED_SSD1306_ERR_PARAM);
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, &default_cfg, NULL),
              OLED_SSD1306_ERR_PARAM);
    ASSERT_EQ(oled_ssd1306_flush(NULL), OLED_SSD1306_ERR_PARAM);
}

TEST(test_set_get_pixel)
{
    setup();
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 10, 20));
    oled_ssd1306_set_pixel(&dev, 10, 20, true);
    ASSERT_TRUE(oled_ssd1306_get_pixel(&dev, 10, 20));
    oled_ssd1306_set_pixel(&dev, 10, 20, false);
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 10, 20));
}

TEST(test_pixel_out_of_bounds)
{
    setup();
    oled_ssd1306_set_pixel(&dev, 128, 0, true);
    oled_ssd1306_set_pixel(&dev, 0, 64, true);
    oled_ssd1306_set_pixel(&dev, 255, 255, true);
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 128, 0));
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 0, 64));
}

TEST(test_clear)
{
    setup();
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_set_pixel(&dev, 64, 32, true);
    oled_ssd1306_clear(&dev);
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 0, 0));
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 64, 32));
}

TEST(test_fill)
{
    setup();
    oled_ssd1306_fill(&dev, true);
    ASSERT_TRUE(oled_ssd1306_get_pixel(&dev, 0, 0));
    ASSERT_TRUE(oled_ssd1306_get_pixel(&dev, 127, 63));

    oled_ssd1306_fill(&dev, false);
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 0, 0));
}

TEST(test_flush)
{
    setup();
    mock_reset();
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_err_t e = oled_ssd1306_flush(&dev);
    ASSERT_EQ(e, OLED_SSD1306_OK);
    ASSERT_TRUE(mock_cmd_count > 0);
    ASSERT_EQ(mock_data_count, 1);
    ASSERT_EQ(last_data_len, 1024);
    ASSERT_TRUE(last_data_ptr == framebuf);
}

TEST(test_flush_i2c_fail)
{
    setup();
    mock_reset();
    mock_fail_at = 0;
    oled_ssd1306_err_t e = oled_ssd1306_flush(&dev);
    ASSERT_EQ(e, OLED_SSD1306_ERR_I2C);
}

TEST(test_pixel_bit_packing)
{
    setup();
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        oled_ssd1306_set_pixel(&dev, 0, bit, true);
    }
    ASSERT_EQ(framebuf[0], 0xFF);
}

TEST(test_multiple_pixels_same_byte)
{
    setup();
    oled_ssd1306_set_pixel(&dev, 5, 0, true);
    oled_ssd1306_set_pixel(&dev, 5, 3, true);
    ASSERT_TRUE(oled_ssd1306_get_pixel(&dev, 5, 0));
    ASSERT_TRUE(oled_ssd1306_get_pixel(&dev, 5, 3));
    ASSERT_TRUE(!oled_ssd1306_get_pixel(&dev, 5, 1));
}

TEST(test_invalid_height)
{
    mock_reset();
    oled_ssd1306_cfg_t bad_cfg = { .addr = 0x3C, .width = 128, .height = 7 };
    oled_ssd1306_t d;
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, &bad_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM);
}

TEST(test_null_clear_fill)
{
    oled_ssd1306_clear(NULL);
    oled_ssd1306_fill(NULL, true);
    oled_ssd1306_set_pixel(NULL, 0, 0, true);
    ASSERT_TRUE(!oled_ssd1306_get_pixel(NULL, 0, 0));
    ASSERT_TRUE(1);
}

TEST(test_init_com_pin_config)
{
    /* 64px height → COM pin config = 0x12 (alternative COM pin mapping) */
    mock_reset();
    oled_ssd1306_t d64;
    oled_ssd1306_cfg_t cfg64 = { .addr = OLED_SSD1306_ADDR_A, .width = 128, .height = 64 };
    uint8_t fb64[OLED_SSD1306_BUF_SIZE(128, 64)];
    ASSERT_EQ(oled_ssd1306_init(&d64, &mock_i2c, &cfg64, fb64), OLED_SSD1306_OK);
    int found_da = 0;
    for (int i = 0; i < cmd_log_count - 1; i++) {
        if (cmd_log[i] == 0xDA) { ASSERT_EQ(cmd_log[i+1], 0x12); found_da = 1; break; }
    }
    ASSERT_TRUE(found_da);

    /* 32px height → COM pin config = 0x02 (sequential COM pin mapping) */
    mock_reset();
    oled_ssd1306_t d32;
    oled_ssd1306_cfg_t cfg32 = { .addr = OLED_SSD1306_ADDR_A, .width = 128, .height = 32 };
    uint8_t fb32[OLED_SSD1306_BUF_SIZE(128, 32)];
    ASSERT_EQ(oled_ssd1306_init(&d32, &mock_i2c, &cfg32, fb32), OLED_SSD1306_OK);
    found_da = 0;
    for (int i = 0; i < cmd_log_count - 1; i++) {
        if (cmd_log[i] == 0xDA) { ASSERT_EQ(cmd_log[i+1], 0x02); found_da = 1; break; }
    }
    ASSERT_TRUE(found_da);
}

TEST(test_display_off_sends_0xAE)
{
    setup();
    mock_reset();
    oled_ssd1306_err_t e = oled_ssd1306_display_off(&dev);
    ASSERT_EQ(e, OLED_SSD1306_OK);
    ASSERT_EQ(last_cmd, 0xAE);
}

TEST(test_display_on_sends_0xAF)
{
    setup();
    mock_reset();
    oled_ssd1306_err_t e = oled_ssd1306_display_on(&dev);
    ASSERT_EQ(e, OLED_SSD1306_OK);
    ASSERT_EQ(last_cmd, 0xAF);
}

TEST(test_display_on_off_null_param)
{
    ASSERT_EQ(oled_ssd1306_display_on(NULL), OLED_SSD1306_ERR_PARAM);
    ASSERT_EQ(oled_ssd1306_display_off(NULL), OLED_SSD1306_ERR_PARAM);
}

TEST_MAIN("oled_ssd1306")
    RUN(test_init_ok);
    RUN(test_init_com_pin_config);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_set_get_pixel);
    RUN(test_pixel_out_of_bounds);
    RUN(test_clear);
    RUN(test_fill);
    RUN(test_flush);
    RUN(test_flush_i2c_fail);
    RUN(test_pixel_bit_packing);
    RUN(test_multiple_pixels_same_byte);
    RUN(test_invalid_height);
    RUN(test_null_clear_fill);
    RUN(test_display_off_sends_0xAE);
    RUN(test_display_on_sends_0xAF);
    RUN(test_display_on_off_null_param);
TEST_MAIN_END()

