// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "st7735.h"

/* ---------- mock SPI ---------- */

#define CMD_LOG_SIZE  512
#define DATA_LOG_SIZE 4096

static uint8_t cmd_log[CMD_LOG_SIZE];
static int     cmd_log_idx;
static uint8_t data_log[DATA_LOG_SIZE];
static int     data_log_idx;
static bool    dc_state;       /* false=cmd, true=data */
static bool    cs_state;       /* false=selected, true=deselected */
static int     xfer_count;
static int     delay_total_ms;
static int     delay_call_count;
static int     reset_count;
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static void mock_reset(void)
{
    memset(cmd_log, 0, sizeof(cmd_log));
    cmd_log_idx      = 0;
    memset(data_log, 0, sizeof(data_log));
    data_log_idx     = 0;
    dc_state         = true;
    cs_state         = true;
    xfer_count       = 0;
    delay_total_ms   = 0;
    delay_call_count = 0;
    reset_count      = 0;
    mock_fail_next   = 0;
    mock_op_count    = 0;
    mock_fail_at     = -1;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_cs_select(void *ctx)
{
    (void)ctx;
    cs_state = false;
    return true;
}

static void mock_cs_deselect(void *ctx)
{
    (void)ctx;
    cs_state = true;
}

static bool mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                           uint16_t len)
{
    (void)ctx;
    if (mock_should_fail()) return false;
    xfer_count++;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t byte = tx ? tx[i] : 0;
        if (!dc_state) {
            /* command mode */
            if (cmd_log_idx < CMD_LOG_SIZE) {
                cmd_log[cmd_log_idx++] = byte;
            }
        } else {
            /* data mode */
            if (data_log_idx < DATA_LOG_SIZE) {
                data_log[data_log_idx++] = byte;
            }
        }
        if (rx) rx[i] = 0xFF;
    }
    return true;
}

static void mock_dc_command(void *ctx) { (void)ctx; dc_state = false; }
static void mock_dc_data(void *ctx)    { (void)ctx; dc_state = true; }

static void mock_hw_reset(void *ctx)
{
    (void)ctx;
    reset_count++;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    delay_total_ms += (int)ms;
    delay_call_count++;
}

static const st7735_spi_t mock_spi = {
    .ctx         = NULL,
    .cs_select   = mock_cs_select,
    .cs_deselect = mock_cs_deselect,
    .transfer    = mock_transfer,
    .dc_command  = mock_dc_command,
    .dc_data     = mock_dc_data,
    .reset       = mock_hw_reset,
    .delay_ms    = mock_delay_ms,
};

static st7735_t dev;

static st7735_err_t setup(void)
{
    mock_reset();
    return st7735_init(&dev, &mock_spi);
}

/* Helper: find cmd in cmd_log */
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
    st7735_err_t e = setup();
    ASSERT_EQ(e, ST7735_OK);
}

TEST(test_init_sequence)
{
    setup();
    /* Must send: SLPOUT, COLMOD, MADCTL, NORON, DISPON in order */
    int pos_slpout = find_cmd((uint8_t)ST7735_CMD_SLPOUT);
    int pos_colmod = find_cmd((uint8_t)ST7735_CMD_COLMOD);
    int pos_madctl = find_cmd((uint8_t)ST7735_CMD_MADCTL);
    int pos_noron  = find_cmd((uint8_t)ST7735_CMD_NORON);
    int pos_dispon = find_cmd((uint8_t)ST7735_CMD_DISPON);

    ASSERT_TRUE(pos_slpout >= 0);
    ASSERT_TRUE(pos_colmod >= 0);
    ASSERT_TRUE(pos_madctl >= 0);
    ASSERT_TRUE(pos_noron >= 0);
    ASSERT_TRUE(pos_dispon >= 0);

    /* Verify order */
    ASSERT_TRUE(pos_slpout < pos_colmod);
    ASSERT_TRUE(pos_colmod < pos_madctl);
    ASSERT_TRUE(pos_madctl < pos_noron);
    ASSERT_TRUE(pos_noron < pos_dispon);

    /* HW reset must have been called */
    ASSERT_TRUE(reset_count >= 1);

    /* Delay calls for reset, sleep out, etc. */
    ASSERT_TRUE(delay_call_count >= 3);
}

TEST(test_draw_pixel_00)
{
    setup();
    mock_reset();
    st7735_err_t e = st7735_draw_pixel(&dev, 0, 0, 0xF800);
    ASSERT_EQ(e, ST7735_OK);
    /* Should have sent CASET, RASET, RAMWR */
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_RASET) >= 0);
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_RAMWR) >= 0);
}

TEST(test_fill_screen)
{
    setup();
    mock_reset();
    st7735_err_t e = st7735_fill_screen(&dev, 0x001F);
    ASSERT_EQ(e, ST7735_OK);
    /* CASET + RASET + RAMWR must be sent */
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_RAMWR) >= 0);
}

TEST(test_fill_rect)
{
    setup();
    mock_reset();
    st7735_err_t e = st7735_fill_rect(&dev, 10, 20, 5, 3, 0xABCD);
    ASSERT_EQ(e, ST7735_OK);
    /* Window: CASET data = (10, 14), RASET data = (20, 22) */
    ASSERT_EQ(data_log[0], 0);    /* x0 high */
    ASSERT_EQ(data_log[1], 10);   /* x0 low */
    ASSERT_EQ(data_log[2], 0);    /* x1 high */
    ASSERT_EQ(data_log[3], 14);   /* x1 low */
    ASSERT_EQ(data_log[4], 0);    /* y0 high */
    ASSERT_EQ(data_log[5], 20);   /* y0 low */
    ASSERT_EQ(data_log[6], 0);    /* y1 high */
    ASSERT_EQ(data_log[7], 22);   /* y1 low */
}

TEST(test_out_of_bounds)
{
    setup();
    /* x out of bounds */
    ASSERT_EQ(st7735_draw_pixel(&dev, 128, 0, 0), ST7735_ERR_PARAM);
    /* y out of bounds */
    ASSERT_EQ(st7735_draw_pixel(&dev, 0, 160, 0), ST7735_ERR_PARAM);
    /* fill_rect overflow */
    ASSERT_EQ(st7735_fill_rect(&dev, 120, 0, 20, 1, 0), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_fill_rect(&dev, 0, 150, 1, 20, 0), ST7735_ERR_PARAM);
    /* fill_rect zero size */
    ASSERT_EQ(st7735_fill_rect(&dev, 0, 0, 0, 1, 0), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_fill_rect(&dev, 0, 0, 1, 0, 0), ST7735_ERR_PARAM);
    /* set_window invalid coords */
    ASSERT_EQ(st7735_set_window(&dev, 50, 0, 10, 100), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_set_window(&dev, 0, 50, 100, 10), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_set_window(&dev, 0, 0, 128, 100), ST7735_ERR_PARAM);
}

TEST(test_rgb565_conversion)
{
    /* Pure red: R=0xFF, G=0, B=0 -> 0xF800 */
    ASSERT_EQ_HEX(st7735_rgb565(0xFF, 0, 0), 0xF800);
    /* Pure green: R=0, G=0xFF, B=0 -> 0x07E0 */
    ASSERT_EQ_HEX(st7735_rgb565(0, 0xFF, 0), 0x07E0);
    /* Pure blue: R=0, G=0, B=0xFF -> 0x001F */
    ASSERT_EQ_HEX(st7735_rgb565(0, 0, 0xFF), 0x001F);
    /* White */
    ASSERT_EQ_HEX(st7735_rgb565(0xFF, 0xFF, 0xFF), 0xFFFF);
    /* Black */
    ASSERT_EQ_HEX(st7735_rgb565(0, 0, 0), 0x0000);
}

TEST(test_spi_failure_propagation)
{
    setup();
    mock_fail_next = 1;
    st7735_err_t e = st7735_draw_pixel(&dev, 0, 0, 0xFFFF);
    ASSERT_EQ(e, ST7735_ERR_SPI);
}

TEST(test_null_rejection)
{
    ASSERT_EQ(st7735_init(NULL, &mock_spi), ST7735_ERR_PARAM);
    st7735_t d;
    ASSERT_EQ(st7735_init(&d, NULL), ST7735_ERR_PARAM);

    ASSERT_EQ(st7735_set_window(NULL, 0, 0, 10, 10), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_write_pixels(NULL, &(uint16_t){0}, 1), ST7735_ERR_PARAM);
    setup();
    ASSERT_EQ(st7735_write_pixels(&dev, NULL, 1), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_write_pixels(&dev, &(uint16_t){0}, 0), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_fill_rect(NULL, 0, 0, 1, 1, 0), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_fill_screen(NULL, 0), ST7735_ERR_PARAM);
    ASSERT_EQ(st7735_draw_pixel(NULL, 0, 0, 0), ST7735_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    st7735_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    st7735_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(st7735_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)ST7735_OK);
    ASSERT_EQ(-1, (int)ST7735_ERR_PARAM);
    ASSERT_EQ(-2, (int)ST7735_ERR_SPI);
}

TEST(test_write_pixels_ok)
{
    setup();
    st7735_err_t e = st7735_set_window(&dev, 0, 0, 1, 0);
    ASSERT_EQ(e, ST7735_OK);
    mock_reset();
    uint16_t px[2] = { 0xF800, 0x07E0 };
    e = st7735_write_pixels(&dev, px, 2);
    ASSERT_EQ(e, ST7735_OK);
    ASSERT_TRUE(find_cmd((uint8_t)ST7735_CMD_RAMWR) >= 0);
    /* Verify pixel data: 0xF800 -> 0xF8, 0x00 and 0x07E0 -> 0x07, 0xE0 */
    ASSERT_EQ(data_log[0], 0xF8);
    ASSERT_EQ(data_log[1], 0x00);
    ASSERT_EQ(data_log[2], 0x07);
    ASSERT_EQ(data_log[3], 0xE0);
}

TEST(test_init_spi_fail)
{
    mock_reset();
    mock_fail_at = 0;
    st7735_t d;
    st7735_err_t e = st7735_init(&d, &mock_spi);
    ASSERT_EQ(e, ST7735_ERR_SPI);
}

TEST_MAIN("st7735")
    RUN(test_init_ok);
    RUN(test_init_sequence);
    RUN(test_draw_pixel_00);
    RUN(test_fill_screen);
    RUN(test_fill_rect);
    RUN(test_out_of_bounds);
    RUN(test_rgb565_conversion);
    RUN(test_spi_failure_propagation);
    RUN(test_null_rejection);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_write_pixels_ok);
    RUN(test_init_spi_fail);
TEST_MAIN_END()
