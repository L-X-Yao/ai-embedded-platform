#include <stdio.h>
#include <string.h>
#include "../oled_ssd1306.h"

static int mock_cmd_count;
static int mock_data_count;
static int mock_fail_next;
static int mock_op_count;
static int mock_fail_at;
static uint8_t last_cmd;
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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((int)(a) == (int)(b), msg)

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

static void test_init_ok(void)
{
    printf("%s\n", __func__);
    mock_reset();
    oled_ssd1306_t d;
    oled_ssd1306_err_t e = oled_ssd1306_init(&d, &mock_i2c, &default_cfg, framebuf);
    ASSERT_EQ(e, OLED_SSD1306_OK, "init ok");
    ASSERT(mock_cmd_count > 10, "init sends multiple commands");
}

static void test_init_i2c_fail(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_fail_at = 0;
    oled_ssd1306_t d;
    oled_ssd1306_err_t e = oled_ssd1306_init(&d, &mock_i2c, &default_cfg, framebuf);
    ASSERT_EQ(e, OLED_SSD1306_ERR_I2C, "init I2C fail");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(oled_ssd1306_init(NULL, &mock_i2c, &default_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM, "null dev");
    oled_ssd1306_t d;
    ASSERT_EQ(oled_ssd1306_init(&d, NULL, &default_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM, "null i2c");
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, NULL, framebuf),
              OLED_SSD1306_ERR_PARAM, "null cfg");
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, &default_cfg, NULL),
              OLED_SSD1306_ERR_PARAM, "null buf");
    ASSERT_EQ(oled_ssd1306_flush(NULL), OLED_SSD1306_ERR_PARAM, "flush null");
}

static void test_set_get_pixel(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT(!oled_ssd1306_get_pixel(&dev, 10, 20), "pixel initially off");
    oled_ssd1306_set_pixel(&dev, 10, 20, true);
    ASSERT(oled_ssd1306_get_pixel(&dev, 10, 20), "pixel on after set");
    oled_ssd1306_set_pixel(&dev, 10, 20, false);
    ASSERT(!oled_ssd1306_get_pixel(&dev, 10, 20), "pixel off after clear");
}

static void test_pixel_out_of_bounds(void)
{
    printf("%s\n", __func__);
    setup();
    oled_ssd1306_set_pixel(&dev, 128, 0, true);
    oled_ssd1306_set_pixel(&dev, 0, 64, true);
    oled_ssd1306_set_pixel(&dev, 255, 255, true);
    ASSERT(!oled_ssd1306_get_pixel(&dev, 128, 0), "OOB x ignored");
    ASSERT(!oled_ssd1306_get_pixel(&dev, 0, 64), "OOB y ignored");
}

static void test_clear(void)
{
    printf("%s\n", __func__);
    setup();
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_set_pixel(&dev, 64, 32, true);
    oled_ssd1306_clear(&dev);
    ASSERT(!oled_ssd1306_get_pixel(&dev, 0, 0), "cleared p1");
    ASSERT(!oled_ssd1306_get_pixel(&dev, 64, 32), "cleared p2");
}

static void test_fill(void)
{
    printf("%s\n", __func__);
    setup();
    oled_ssd1306_fill(&dev, true);
    ASSERT(oled_ssd1306_get_pixel(&dev, 0, 0), "fill on: corner");
    ASSERT(oled_ssd1306_get_pixel(&dev, 127, 63), "fill on: last pixel");

    oled_ssd1306_fill(&dev, false);
    ASSERT(!oled_ssd1306_get_pixel(&dev, 0, 0), "fill off: corner");
}

static void test_flush(void)
{
    printf("%s\n", __func__);
    setup();
    mock_reset();
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_err_t e = oled_ssd1306_flush(&dev);
    ASSERT_EQ(e, OLED_SSD1306_OK, "flush ok");
    ASSERT(mock_cmd_count > 0, "flush sends column/page commands");
    ASSERT_EQ(mock_data_count, 1, "flush sends framebuffer data");
    ASSERT_EQ(last_data_len, 1024, "data length = 128*8 pages");
    ASSERT(last_data_ptr == framebuf, "data pointer is framebuffer");
}

static void test_flush_i2c_fail(void)
{
    printf("%s\n", __func__);
    setup();
    mock_reset();
    mock_fail_at = 0;
    oled_ssd1306_err_t e = oled_ssd1306_flush(&dev);
    ASSERT_EQ(e, OLED_SSD1306_ERR_I2C, "flush I2C fail");
}

static void test_pixel_bit_packing(void)
{
    printf("%s\n", __func__);
    setup();
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        oled_ssd1306_set_pixel(&dev, 0, bit, true);
    }
    ASSERT_EQ(framebuf[0], 0xFF, "8 vertical pixels pack into one byte");
}

static void test_multiple_pixels_same_byte(void)
{
    printf("%s\n", __func__);
    setup();
    oled_ssd1306_set_pixel(&dev, 5, 0, true);
    oled_ssd1306_set_pixel(&dev, 5, 3, true);
    ASSERT(oled_ssd1306_get_pixel(&dev, 5, 0), "bit 0 set");
    ASSERT(oled_ssd1306_get_pixel(&dev, 5, 3), "bit 3 set");
    ASSERT(!oled_ssd1306_get_pixel(&dev, 5, 1), "bit 1 not set");
}

static void test_invalid_height(void)
{
    printf("%s\n", __func__);
    mock_reset();
    oled_ssd1306_cfg_t bad_cfg = { .addr = 0x3C, .width = 128, .height = 7 };
    oled_ssd1306_t d;
    ASSERT_EQ(oled_ssd1306_init(&d, &mock_i2c, &bad_cfg, framebuf),
              OLED_SSD1306_ERR_PARAM, "height not multiple of 8");
}

static void test_null_clear_fill(void)
{
    printf("%s\n", __func__);
    oled_ssd1306_clear(NULL);
    oled_ssd1306_fill(NULL, true);
    oled_ssd1306_set_pixel(NULL, 0, 0, true);
    ASSERT(!oled_ssd1306_get_pixel(NULL, 0, 0), "get_pixel null");
    ASSERT(1, "null ops don't crash");
}

int main(void)
{
    printf("test_oled_ssd1306\n");
    test_init_ok();
    test_init_i2c_fail();
    test_null_safety();
    test_set_get_pixel();
    test_pixel_out_of_bounds();
    test_clear();
    test_fill();
    test_flush();
    test_flush_i2c_fail();
    test_pixel_bit_packing();
    test_multiple_pixels_same_byte();
    test_invalid_height();
    test_null_clear_fill();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
