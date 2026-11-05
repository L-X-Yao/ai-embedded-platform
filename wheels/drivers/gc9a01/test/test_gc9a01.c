// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "gc9a01.h"

/* ---------- mock SPI ---------- */

#define CMD_LOG_SIZE  512
#define DATA_LOG_SIZE 4096

static uint8_t cmd_log[CMD_LOG_SIZE];
static int     cmd_log_idx;
static uint8_t data_log[DATA_LOG_SIZE];
static int     data_log_idx;
static bool    dc_state;       /* false=cmd, true=data */
static bool    cs_state;       /* false=low(asserted), true=high */
static bool    rst_state;      /* true=normal, false=reset */
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

static void mock_dc_cmd(void *ctx)
{
    (void)ctx;
    dc_state = false;
}

static void mock_dc_data(void *ctx)
{
    (void)ctx;
    dc_state = true;
}

static void mock_rst_low(void *ctx)
{
    (void)ctx;
    rst_state = false;
}

static void mock_rst_high(void *ctx)
{
    (void)ctx;
    rst_state = true;
}

static void mock_delay_ms(void *ctx, uint16_t ms)
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

static const gc9a01_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .dc_cmd   = mock_dc_cmd,
    .dc_data  = mock_dc_data,
    .rst_low  = mock_rst_low,
    .rst_high = mock_rst_high,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static gc9a01_t dev;

static gc9a01_err_t setup(void)
{
    mock_reset();
    return gc9a01_init(&dev, &mock_spi);
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
    gc9a01_err_t e = setup();
    ASSERT_EQ(e, GC9A01_OK);
}

TEST(test_init_sends_reset_sequence)
{
    setup();
    /* delay_ms called for: HW reset(10), HW reset release(120), SW reset(120),
     * sleep out(120), display on(20) => 5 calls, total >= 390ms */
    ASSERT_TRUE(delay_call_count >= 5);
    ASSERT_TRUE(delay_total_ms >= 390);
}

TEST(test_init_sends_swreset)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_SWRESET) >= 0);
}

TEST(test_init_sends_slpout)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_SLPOUT) >= 0);
}

TEST(test_init_sends_colmod)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_COLMOD) >= 0);
}

TEST(test_init_sends_madctl)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_MADCTL) >= 0);
}

TEST(test_init_sends_invon)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_INVON) >= 0);
}

TEST(test_init_sends_dispon)
{
    setup();
    ASSERT_TRUE(find_cmd(GC9A01_CMD_DISPON) >= 0);
}

TEST(test_init_sets_dimensions)
{
    setup();
    ASSERT_EQ(dev.width, 240);
    ASSERT_EQ(dev.height, 240);
    ASSERT_EQ(dev.rotation, 0);
}

TEST(test_init_colmod_rgb565)
{
    setup();
    /* COLMOD data byte should be 0x05 (RGB565) */
    /* Find the COLMOD command index, then the next data byte */
    int colmod_idx = find_cmd(GC9A01_CMD_COLMOD);
    ASSERT_TRUE(colmod_idx >= 0);
    /* The data byte 0x05 should appear in the data_log */
    bool found_rgb565 = false;
    for (int i = 0; i < data_log_idx; i++) {
        if (data_log[i] == GC9A01_COLMOD_RGB565) {
            found_rgb565 = true;
            break;
        }
    }
    ASSERT_TRUE(found_rgb565);
}

TEST(test_init_command_order)
{
    setup();
    /* SWRESET must come before SLPOUT, SLPOUT before DISPON */
    int swreset_idx = find_cmd(GC9A01_CMD_SWRESET);
    int slpout_idx  = find_cmd(GC9A01_CMD_SLPOUT);
    int dispon_idx  = find_cmd(GC9A01_CMD_DISPON);
    ASSERT_TRUE(swreset_idx < slpout_idx);
    ASSERT_TRUE(slpout_idx < dispon_idx);
}

/* ========== null / param safety tests ========== */

TEST(test_init_null_dev)
{
    ASSERT_EQ(gc9a01_init(NULL, &mock_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    gc9a01_t d;
    ASSERT_EQ(gc9a01_init(&d, NULL), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_dc_cmd)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.dc_cmd = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_dc_data)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.dc_data = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_rst_low)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.rst_low = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_rst_high)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.rst_high = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_delay)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.delay_ms = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_cs_low)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.cs_low = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

TEST(test_init_missing_xfer)
{
    gc9a01_t d;
    gc9a01_spi_t bad_spi = mock_spi;
    bad_spi.xfer = NULL;
    ASSERT_EQ(gc9a01_init(&d, &bad_spi), GC9A01_ERR_PARAM);
}

/* ========== set_window tests ========== */

TEST(test_set_window_ok)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_set_window(&dev, 0, 0, 239, 239);
    ASSERT_EQ(e, GC9A01_OK);
    /* Should have sent CASET and RASET */
    ASSERT_TRUE(find_cmd(GC9A01_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_RASET) >= 0);
}

TEST(test_set_window_small_region)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_set_window(&dev, 10, 20, 50, 60);
    ASSERT_EQ(e, GC9A01_OK);
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

TEST(test_set_window_null_dev)
{
    ASSERT_EQ(gc9a01_set_window(NULL, 0, 0, 10, 10), GC9A01_ERR_PARAM);
}

TEST(test_set_window_inverted_coords)
{
    setup();
    /* x0 > x1 */
    ASSERT_EQ(gc9a01_set_window(&dev, 50, 0, 10, 100), GC9A01_ERR_PARAM);
    /* y0 > y1 */
    ASSERT_EQ(gc9a01_set_window(&dev, 0, 50, 100, 10), GC9A01_ERR_PARAM);
}

TEST(test_set_window_out_of_bounds)
{
    setup();
    /* x1 >= width (240) */
    ASSERT_EQ(gc9a01_set_window(&dev, 0, 0, 240, 100), GC9A01_ERR_PARAM);
    /* y1 >= height (240) */
    ASSERT_EQ(gc9a01_set_window(&dev, 0, 0, 100, 240), GC9A01_ERR_PARAM);
}

TEST(test_set_window_uninit_dev)
{
    gc9a01_t d;
    memset(&d, 0, sizeof(d));
    /* spi is NULL after zero-init */
    ASSERT_EQ(gc9a01_set_window(&d, 0, 0, 10, 10), GC9A01_ERR_PARAM);
}

/* ========== write_pixels tests ========== */

TEST(test_write_pixels_ok)
{
    setup();
    gc9a01_set_window(&dev, 0, 0, 1, 0);
    mock_reset();
    uint16_t px[2] = { 0xF800, 0x07E0 };  /* red + green in RGB565 */
    gc9a01_err_t e = gc9a01_write_pixels(&dev, px, 2);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_RAMWR) >= 0);
    /* verify pixel bytes sent big-endian */
    ASSERT_EQ(data_log[0], 0xF8);  /* red high */
    ASSERT_EQ(data_log[1], 0x00);  /* red low  */
    ASSERT_EQ(data_log[2], 0x07);  /* green high */
    ASSERT_EQ(data_log[3], 0xE0);  /* green low  */
}

TEST(test_write_pixels_null_dev)
{
    uint16_t px[1] = { 0xFFFF };
    ASSERT_EQ(gc9a01_write_pixels(NULL, px, 1), GC9A01_ERR_PARAM);
}

TEST(test_write_pixels_null_data)
{
    setup();
    ASSERT_EQ(gc9a01_write_pixels(&dev, NULL, 1), GC9A01_ERR_PARAM);
}

TEST(test_write_pixels_zero_count)
{
    setup();
    uint16_t px[1] = { 0xFFFF };
    ASSERT_EQ(gc9a01_write_pixels(&dev, px, 0), GC9A01_ERR_PARAM);
}

TEST(test_write_pixels_single)
{
    setup();
    gc9a01_set_window(&dev, 0, 0, 0, 0);
    mock_reset();
    uint16_t px = 0xABCD;
    gc9a01_err_t e = gc9a01_write_pixels(&dev, &px, 1);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_EQ(data_log[0], 0xAB);
    ASSERT_EQ(data_log[1], 0xCD);
}

/* ========== fill tests ========== */

TEST(test_fill_ok)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_fill(&dev, 0xF800);
    ASSERT_EQ(e, GC9A01_OK);
    /* Should have sent CASET + RASET + RAMWR */
    ASSERT_TRUE(find_cmd(GC9A01_CMD_CASET) >= 0);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_RASET) >= 0);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_RAMWR) >= 0);
}

TEST(test_fill_pixel_data)
{
    setup();
    mock_reset();
    gc9a01_fill(&dev, 0xABCD);
    /* After CASET(4 bytes) + RASET(4 bytes) data, pixel data starts at offset 8 */
    ASSERT_EQ(data_log[8], 0xAB);
    ASSERT_EQ(data_log[9], 0xCD);
}

TEST(test_fill_null_dev)
{
    ASSERT_EQ(gc9a01_fill(NULL, 0xFFFF), GC9A01_ERR_PARAM);
}

TEST(test_fill_xfer_count)
{
    setup();
    mock_reset();
    gc9a01_fill(&dev, 0x001F);
    /* CASET: 1 cmd + 4 data = 5 xfer
     * RASET: 1 cmd + 4 data = 5 xfer
     * RAMWR: 1 cmd + 240*240*2 data = 1 + 115200 = 115201 xfer
     * Total: 5 + 5 + 115201 = 115211 */
    ASSERT_EQ(xfer_count, 115211);
}

/* ========== rotation tests ========== */

/* V1.0 p.127: the 36h parameter is MY MX MV ML BGR MH 0 0, from D7 to D0 */
TEST(test_madctl_bits)
{
    ASSERT_EQ(GC9A01_MADCTL_MY, 0x80);
    ASSERT_EQ(GC9A01_MADCTL_MX, 0x40);
    ASSERT_EQ(GC9A01_MADCTL_MV, 0x20);
    ASSERT_EQ(GC9A01_MADCTL_ML, 0x10);
    ASSERT_EQ(GC9A01_MADCTL_BGR, 0x08);
    ASSERT_EQ(GC9A01_MADCTL_MH, 0x04);
}

TEST(test_rotation_0)
{
    setup();
    gc9a01_err_t e = gc9a01_set_rotation(&dev, 0);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_EQ(dev.width, 240);
    ASSERT_EQ(dev.height, 240);
    ASSERT_EQ(dev.rotation, 0);
}

TEST(test_rotation_1)
{
    setup();
    gc9a01_err_t e = gc9a01_set_rotation(&dev, 1);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_EQ(dev.width, 240);
    ASSERT_EQ(dev.height, 240);
    ASSERT_EQ(dev.rotation, 1);
}

TEST(test_rotation_2)
{
    setup();
    gc9a01_err_t e = gc9a01_set_rotation(&dev, 2);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_EQ(dev.rotation, 2);
}

TEST(test_rotation_3)
{
    setup();
    gc9a01_err_t e = gc9a01_set_rotation(&dev, 3);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_EQ(dev.rotation, 3);
}

TEST(test_rotation_sends_madctl)
{
    setup();
    mock_reset();
    gc9a01_set_rotation(&dev, 1);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_MADCTL) >= 0);
    /* MADCTL for rotation 1: MV | MY = 0x20 | 0x80 = 0xA0 */
    ASSERT_EQ(data_log[0], 0xA0);
}

TEST(test_rotation_2_madctl)
{
    setup();
    mock_reset();
    gc9a01_set_rotation(&dev, 2);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_MADCTL) >= 0);
    /* MADCTL for rotation 2: MX | MY = 0x40 | 0x80 = 0xC0 */
    ASSERT_EQ(data_log[0], 0xC0);
}

TEST(test_rotation_3_madctl)
{
    setup();
    mock_reset();
    gc9a01_set_rotation(&dev, 3);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_MADCTL) >= 0);
    /* MADCTL for rotation 3: MV | MX = 0x20 | 0x40 = 0x60 */
    ASSERT_EQ(data_log[0], 0x60);
}

TEST(test_rotation_0_madctl)
{
    setup();
    mock_reset();
    gc9a01_set_rotation(&dev, 0);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_MADCTL) >= 0);
    /* MADCTL for rotation 0: 0x00 */
    ASSERT_EQ(data_log[0], 0x00);
}

TEST(test_rotation_invalid)
{
    setup();
    ASSERT_EQ(gc9a01_set_rotation(&dev, 4), GC9A01_ERR_PARAM);
    ASSERT_EQ(gc9a01_set_rotation(&dev, 255), GC9A01_ERR_PARAM);
}

TEST(test_rotation_null_dev)
{
    ASSERT_EQ(gc9a01_set_rotation(NULL, 0), GC9A01_ERR_PARAM);
}

/* ========== sleep tests ========== */

TEST(test_sleep_enter)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_sleep(&dev, true);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_SLPIN) >= 0);
    /* V1.0 p.103: SLPOUT can only follow 120 ms after SLPIN */
    ASSERT_TRUE(delay_total_ms >= 120);
}

TEST(test_sleep_exit)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_sleep(&dev, false);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_SLPOUT) >= 0);
    /* exit sleep requires 120ms delay */
    ASSERT_TRUE(delay_total_ms >= 120);
}

TEST(test_sleep_null_dev)
{
    ASSERT_EQ(gc9a01_sleep(NULL, true), GC9A01_ERR_PARAM);
    ASSERT_EQ(gc9a01_sleep(NULL, false), GC9A01_ERR_PARAM);
}

/* ========== display on/off tests ========== */

TEST(test_display_on)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_display(&dev, true);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_DISPON) >= 0);
}

TEST(test_display_off)
{
    setup();
    mock_reset();
    gc9a01_err_t e = gc9a01_display(&dev, false);
    ASSERT_EQ(e, GC9A01_OK);
    ASSERT_TRUE(find_cmd(GC9A01_CMD_DISPOFF) >= 0);
}

TEST(test_display_null_dev)
{
    ASSERT_EQ(gc9a01_display(NULL, true), GC9A01_ERR_PARAM);
    ASSERT_EQ(gc9a01_display(NULL, false), GC9A01_ERR_PARAM);
}

/* ========== rgb565 tests ========== */

TEST(test_rgb565_red)
{
    uint16_t c = gc9a01_rgb565(255, 0, 0);
    /* R=255 => 0xF8 << 8 = 0xF800 */
    ASSERT_EQ_HEX(c, 0xF800);
}

TEST(test_rgb565_green)
{
    uint16_t c = gc9a01_rgb565(0, 255, 0);
    /* G=255 => 0xFC << 3 = 0x07E0 */
    ASSERT_EQ_HEX(c, 0x07E0);
}

TEST(test_rgb565_blue)
{
    uint16_t c = gc9a01_rgb565(0, 0, 255);
    /* B=255 => 255 >> 3 = 31 = 0x001F */
    ASSERT_EQ_HEX(c, 0x001F);
}

TEST(test_rgb565_white)
{
    uint16_t c = gc9a01_rgb565(255, 255, 255);
    ASSERT_EQ_HEX(c, 0xFFFF);
}

TEST(test_rgb565_black)
{
    uint16_t c = gc9a01_rgb565(0, 0, 0);
    ASSERT_EQ_HEX(c, 0x0000);
}

TEST(test_rgb565_mixed)
{
    /* R=128, G=64, B=32 */
    uint16_t c = gc9a01_rgb565(128, 64, 32);
    /* R: 128 & 0xF8 = 0x80, << 8 = 0x8000
     * G: 64  & 0xFC = 0x40, << 3 = 0x0200
     * B: 32  >> 3   = 4          = 0x0004
     * Total: 0x8204 */
    ASSERT_EQ_HEX(c, 0x8204);
}

/* ========== error code tests ========== */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)GC9A01_OK);
    ASSERT_EQ(-1, (int)GC9A01_ERR_PARAM);
    ASSERT_EQ(-2, (int)GC9A01_ERR_SPI);
}

/* ========== runner ========== */

TEST_MAIN("gc9a01")
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
    RUN(test_init_colmod_rgb565);
    RUN(test_init_command_order);
    /* null / param safety */
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_missing_dc_cmd);
    RUN(test_init_missing_dc_data);
    RUN(test_init_missing_rst_low);
    RUN(test_init_missing_rst_high);
    RUN(test_init_missing_delay);
    RUN(test_init_missing_cs_low);
    RUN(test_init_missing_xfer);
    /* set_window */
    RUN(test_set_window_ok);
    RUN(test_set_window_small_region);
    RUN(test_set_window_null_dev);
    RUN(test_set_window_inverted_coords);
    RUN(test_set_window_out_of_bounds);
    RUN(test_set_window_uninit_dev);
    /* write_pixels */
    RUN(test_write_pixels_ok);
    RUN(test_write_pixels_null_dev);
    RUN(test_write_pixels_null_data);
    RUN(test_write_pixels_zero_count);
    RUN(test_write_pixels_single);
    /* fill */
    RUN(test_fill_ok);
    RUN(test_fill_pixel_data);
    RUN(test_fill_null_dev);
    RUN(test_fill_xfer_count);
    /* rotation */
    RUN(test_madctl_bits);
    RUN(test_rotation_0);
    RUN(test_rotation_1);
    RUN(test_rotation_2);
    RUN(test_rotation_3);
    RUN(test_rotation_sends_madctl);
    RUN(test_rotation_2_madctl);
    RUN(test_rotation_3_madctl);
    RUN(test_rotation_0_madctl);
    RUN(test_rotation_invalid);
    RUN(test_rotation_null_dev);
    /* sleep */
    RUN(test_sleep_enter);
    RUN(test_sleep_exit);
    RUN(test_sleep_null_dev);
    /* display on/off */
    RUN(test_display_on);
    RUN(test_display_off);
    RUN(test_display_null_dev);
    /* rgb565 */
    RUN(test_rgb565_red);
    RUN(test_rgb565_green);
    RUN(test_rgb565_blue);
    RUN(test_rgb565_white);
    RUN(test_rgb565_black);
    RUN(test_rgb565_mixed);
    /* error codes */
    RUN(test_error_codes);
TEST_MAIN_END()
