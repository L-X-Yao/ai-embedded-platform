#include "test_harness.h"
#include <string.h>
#include "ili9341.h"

/* ---------- mock SPI with DC/RST pins ---------- */

static int     mock_fail_at;
static int     mock_op_count;
static uint8_t mock_cs_state;
static bool    mock_dc_state;
static bool    mock_rst_state;
static uint32_t mock_delay_total;

/* Command/data log to verify init sequence */
#define LOG_SIZE 512
static uint8_t cmd_log[LOG_SIZE];
static int     cmd_log_idx;
static uint8_t data_log[LOG_SIZE];
static int     data_log_idx;
/* Track DC state per xfer to distinguish cmd vs data */
static bool    xfer_dc_log[LOG_SIZE];
static int     xfer_total;

static void mock_reset(void)
{
    mock_fail_at   = -1;
    mock_op_count  = 0;
    mock_cs_state  = 1;
    mock_dc_state  = true;
    mock_rst_state = true;
    mock_delay_total = 0;
    cmd_log_idx    = 0;
    data_log_idx   = 0;
    xfer_total     = 0;
    memset(cmd_log, 0, sizeof(cmd_log));
    memset(data_log, 0, sizeof(data_log));
    memset(xfer_dc_log, 0, sizeof(xfer_dc_log));
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static void mock_dc_set(void *ctx, bool level)
{
    (void)ctx;
    mock_dc_state = level;
}

static void mock_rst_set(void *ctx, bool level)
{
    (void)ctx;
    mock_rst_state = level;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += ms;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    mock_op_count++;
    if (xfer_total < LOG_SIZE) {
        xfer_dc_log[xfer_total] = mock_dc_state;
    }
    if (!mock_dc_state && cmd_log_idx < LOG_SIZE) {
        /* Command byte (DC=low) */
        cmd_log[cmd_log_idx++] = tx;
    } else if (mock_dc_state && data_log_idx < LOG_SIZE) {
        /* Data byte (DC=high) */
        data_log[data_log_idx++] = tx;
    }
    xfer_total++;
    return 0xFF;
}

static const ili9341_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .dc_set   = mock_dc_set,
    .rst_set  = mock_rst_set,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ili9341_t dev;
    ASSERT_EQ(ili9341_init(&dev, &mock_spi), ILI9341_OK);
    /* After init: default rotation 0, width=240, height=320 */
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
    ASSERT_EQ(dev.rotation, ILI9341_ROT_0);
}

TEST(test_init_sends_reset)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    /* Check that delay was called (for reset + sleep out) */
    ASSERT_TRUE(mock_delay_total > 0);
}

TEST(test_init_sends_swreset)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    /* SWRESET (0x01) should be in the command log */
    bool found_swreset = false;
    for (int i = 0; i < cmd_log_idx; i++) {
        if (cmd_log[i] == 0x01) { found_swreset = true; break; }
    }
    ASSERT_TRUE(found_swreset);
}

TEST(test_init_sends_slpout)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    bool found_slpout = false;
    for (int i = 0; i < cmd_log_idx; i++) {
        if (cmd_log[i] == 0x11) { found_slpout = true; break; }
    }
    ASSERT_TRUE(found_slpout);
}

TEST(test_init_sends_dispon)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    bool found_dispon = false;
    for (int i = 0; i < cmd_log_idx; i++) {
        if (cmd_log[i] == 0x29) { found_dispon = true; break; }
    }
    ASSERT_TRUE(found_dispon);
}

TEST(test_init_sends_colmod)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    /* COLMOD (0x3A) should be followed by 0x55 (RGB565) */
    bool found_colmod = false;
    for (int i = 0; i < cmd_log_idx; i++) {
        if (cmd_log[i] == 0x3A) { found_colmod = true; break; }
    }
    ASSERT_TRUE(found_colmod);
    /* 0x55 should be in data log */
    bool found_rgb565 = false;
    for (int i = 0; i < data_log_idx; i++) {
        if (data_log[i] == 0x55) { found_rgb565 = true; break; }
    }
    ASSERT_TRUE(found_rgb565);
}

TEST(test_null_safety)
{
    ili9341_t dev;
    ASSERT_EQ(ili9341_init(NULL, &mock_spi), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_init(&dev, NULL), ILI9341_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    ili9341_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ili9341_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ili9341_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)ILI9341_ERR_PARAM);
    ASSERT_EQ(-2, (int)ILI9341_ERR_SPI);
}

TEST(test_rotation_0)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_rotation(&dev, ILI9341_ROT_0), ILI9341_OK);
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
}

TEST(test_rotation_90)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_rotation(&dev, ILI9341_ROT_90), ILI9341_OK);
    ASSERT_EQ(dev.cur_width, 320);
    ASSERT_EQ(dev.cur_height, 240);
}

TEST(test_rotation_180)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_rotation(&dev, ILI9341_ROT_180), ILI9341_OK);
    ASSERT_EQ(dev.cur_width, 240);
    ASSERT_EQ(dev.cur_height, 320);
}

TEST(test_rotation_270)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_rotation(&dev, ILI9341_ROT_270), ILI9341_OK);
    ASSERT_EQ(dev.cur_width, 320);
    ASSERT_EQ(dev.cur_height, 240);
}

TEST(test_rotation_invalid)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_rotation(&dev, (ili9341_rotation_t)4), ILI9341_ERR_PARAM);
}

TEST(test_rotation_null)
{
    ASSERT_EQ(ili9341_set_rotation(NULL, ILI9341_ROT_0), ILI9341_ERR_PARAM);
}

TEST(test_set_window_ok)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_window(&dev, 0, 0, 239, 319), ILI9341_OK);
}

TEST(test_set_window_partial)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_window(&dev, 10, 20, 100, 200), ILI9341_OK);
}

TEST(test_set_window_out_of_bounds)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_set_window(&dev, 0, 0, 240, 0), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_set_window(&dev, 0, 0, 0, 320), ILI9341_ERR_PARAM);
}

TEST(test_set_window_inverted)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    /* x0 > x1 */
    ASSERT_EQ(ili9341_set_window(&dev, 100, 0, 50, 100), ILI9341_ERR_PARAM);
    /* y0 > y1 */
    ASSERT_EQ(ili9341_set_window(&dev, 0, 100, 50, 50), ILI9341_ERR_PARAM);
}

TEST(test_set_window_null)
{
    ASSERT_EQ(ili9341_set_window(NULL, 0, 0, 10, 10), ILI9341_ERR_PARAM);
}

TEST(test_fill_rect)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    int pre_ops = mock_op_count;
    ASSERT_EQ(ili9341_fill_rect(&dev, 0, 0, 2, 2, 0xF800), ILI9341_OK);
    /* 2x2 = 4 pixels x 2 bytes = 8 data xfers + cmd overhead */
    ASSERT_TRUE(mock_op_count > pre_ops);
}

TEST(test_fill_rect_out_of_bounds)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_fill_rect(&dev, 240, 0, 1, 1, 0), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_fill_rect(&dev, 0, 0, 241, 1, 0), ILI9341_ERR_PARAM);
}

TEST(test_fill_rect_zero_size)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_fill_rect(&dev, 0, 0, 0, 10, 0), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_fill_rect(&dev, 0, 0, 10, 0, 0), ILI9341_ERR_PARAM);
}

TEST(test_fill_rect_null)
{
    ASSERT_EQ(ili9341_fill_rect(NULL, 0, 0, 1, 1, 0), ILI9341_ERR_PARAM);
}

TEST(test_draw_pixel)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_draw_pixel(&dev, 120, 160, 0x07E0), ILI9341_OK);
}

TEST(test_draw_pixel_out_of_bounds)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_draw_pixel(&dev, 240, 0, 0), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_draw_pixel(&dev, 0, 320, 0), ILI9341_ERR_PARAM);
}

TEST(test_draw_pixel_null)
{
    ASSERT_EQ(ili9341_draw_pixel(NULL, 0, 0, 0), ILI9341_ERR_PARAM);
}

TEST(test_fill_screen)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(ili9341_fill_screen(&dev, 0x001F), ILI9341_OK);
}

TEST(test_fill_screen_null)
{
    ASSERT_EQ(ili9341_fill_screen(NULL, 0), ILI9341_ERR_PARAM);
}

TEST(test_rgb565_red)
{
    uint16_t c = ili9341_rgb565(255, 0, 0);
    /* Red: 11111 000000 00000 = 0xF800 */
    ASSERT_EQ_HEX(c, 0xF800);
}

TEST(test_rgb565_green)
{
    uint16_t c = ili9341_rgb565(0, 255, 0);
    /* Green: 00000 111111 00000 = 0x07E0 */
    ASSERT_EQ_HEX(c, 0x07E0);
}

TEST(test_rgb565_blue)
{
    uint16_t c = ili9341_rgb565(0, 0, 255);
    /* Blue: 00000 000000 11111 = 0x001F */
    ASSERT_EQ_HEX(c, 0x001F);
}

TEST(test_rgb565_white)
{
    uint16_t c = ili9341_rgb565(255, 255, 255);
    ASSERT_EQ_HEX(c, 0xFFFF);
}

TEST(test_rgb565_black)
{
    uint16_t c = ili9341_rgb565(0, 0, 0);
    ASSERT_EQ_HEX(c, 0x0000);
}

TEST(test_window_after_rotation)
{
    mock_reset();
    ili9341_t dev;
    ili9341_init(&dev, &mock_spi);
    ili9341_set_rotation(&dev, ILI9341_ROT_90);
    /* In 90 rotation, width=320, height=240 */
    ASSERT_EQ(ili9341_set_window(&dev, 0, 0, 319, 239), ILI9341_OK);
    ASSERT_EQ(ili9341_set_window(&dev, 0, 0, 320, 0), ILI9341_ERR_PARAM);
}

TEST_MAIN("ili9341")
    RUN(test_init_ok);
    RUN(test_init_sends_reset);
    RUN(test_init_sends_swreset);
    RUN(test_init_sends_slpout);
    RUN(test_init_sends_dispon);
    RUN(test_init_sends_colmod);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_rotation_0);
    RUN(test_rotation_90);
    RUN(test_rotation_180);
    RUN(test_rotation_270);
    RUN(test_rotation_invalid);
    RUN(test_rotation_null);
    RUN(test_set_window_ok);
    RUN(test_set_window_partial);
    RUN(test_set_window_out_of_bounds);
    RUN(test_set_window_inverted);
    RUN(test_set_window_null);
    RUN(test_fill_rect);
    RUN(test_fill_rect_out_of_bounds);
    RUN(test_fill_rect_zero_size);
    RUN(test_fill_rect_null);
    RUN(test_draw_pixel);
    RUN(test_draw_pixel_out_of_bounds);
    RUN(test_draw_pixel_null);
    RUN(test_fill_screen);
    RUN(test_fill_screen_null);
    RUN(test_rgb565_red);
    RUN(test_rgb565_green);
    RUN(test_rgb565_blue);
    RUN(test_rgb565_white);
    RUN(test_rgb565_black);
    RUN(test_window_after_rotation);
TEST_MAIN_END()
