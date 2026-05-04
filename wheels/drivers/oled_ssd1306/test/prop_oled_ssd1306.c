// SPDX-License-Identifier: MIT
/*
 * prop_oled_ssd1306.c — property-based tests for SSD1306 OLED driver
 *
 * SSD1306 uses write_cmd/write_data (not register-based), so we
 * provide a custom mock.
 *
 * Properties:
 *   1. init+draw+flush never crashes with arbitrary pixel coords
 *   2. write_data called at least once after flush
 *   3. set_pixel/get_pixel roundtrip for valid coordinates
 *   4. clear resets all pixels to 0
 */

#include "test_harness.h"
#include "property_test.h"
#include "oled_ssd1306.h"
#include <string.h>

/* ---------- custom mock for SSD1306 ---------- */

static int mock_cmd_count;
static int mock_data_count;
static int mock_data_bytes;
static int mock_fail_flag;

static bool mock_write_cmd(void *ctx, uint8_t dev_addr, uint8_t cmd)
{
    (void)ctx; (void)dev_addr; (void)cmd;
    if (mock_fail_flag) return false;
    mock_cmd_count++;
    return true;
}

static bool mock_write_data(void *ctx, uint8_t dev_addr,
                            const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)data;
    if (mock_fail_flag) return false;
    mock_data_count++;
    mock_data_bytes += (int)len;
    return true;
}

static const oled_ssd1306_i2c_t prop_i2c = {
    .write_cmd  = mock_write_cmd,
    .write_data = mock_write_data,
    .ctx        = NULL,
};

static void mock_reset(void)
{
    mock_cmd_count  = 0;
    mock_data_count = 0;
    mock_data_bytes = 0;
    mock_fail_flag  = 0;
}

/* Shared framebuffer for 128x64 */
static uint8_t framebuf[128 * 64 / 8];

/* ------------------------------------------------------------------ */

PROPERTY(draw_no_crash, 10000)
{
    mock_reset();

    oled_ssd1306_t dev;
    oled_ssd1306_cfg_t cfg = {
        .addr   = OLED_SSD1306_ADDR_A,
        .width  = 128,
        .height = 64,
    };
    oled_ssd1306_err_t err = oled_ssd1306_init(&dev, &prop_i2c, &cfg,
                                                framebuf);
    if (err != OLED_SSD1306_OK) return;

    /* Draw random pixels at arbitrary coordinates (including out-of-bounds) */
    int n = (int)(ARBI_UINT8(seed) % 20) + 1;
    for (int i = 0; i < n; i++) {
        uint8_t x = ARBI_UINT8(seed);
        uint8_t y = ARBI_UINT8(seed);
        bool on = (ARBI_UINT8(seed) & 1) != 0;
        oled_ssd1306_set_pixel(&dev, x, y, on);
    }

    oled_ssd1306_flush(&dev);
    /* Must not crash */
}

PROPERTY(flush_writes_data, 5000)
{
    mock_reset();

    oled_ssd1306_t dev;
    oled_ssd1306_cfg_t cfg = {
        .addr   = OLED_SSD1306_ADDR_A,
        .width  = 128,
        .height = 64,
    };
    oled_ssd1306_err_t err = oled_ssd1306_init(&dev, &prop_i2c, &cfg,
                                                framebuf);
    if (err != OLED_SSD1306_OK) return;

    /* Set a random pixel */
    uint8_t x = ARBI_UINT8(seed) % 128;
    uint8_t y = ARBI_UINT8(seed) % 64;
    oled_ssd1306_set_pixel(&dev, x, y, true);

    mock_data_count = 0;
    mock_data_bytes = 0;
    err = oled_ssd1306_flush(&dev);
    if (err != OLED_SSD1306_OK) return;

    PROP_ASSERT(mock_data_count > 0);
    PROP_ASSERT(mock_data_bytes > 0);
}

PROPERTY(pixel_roundtrip, 5000)
{
    mock_reset();

    oled_ssd1306_t dev;
    oled_ssd1306_cfg_t cfg = {
        .addr   = OLED_SSD1306_ADDR_A,
        .width  = 128,
        .height = 64,
    };
    oled_ssd1306_err_t err = oled_ssd1306_init(&dev, &prop_i2c, &cfg,
                                                framebuf);
    if (err != OLED_SSD1306_OK) return;

    /* Set pixel and read it back */
    uint8_t x = ARBI_UINT8(seed) % 128;
    uint8_t y = ARBI_UINT8(seed) % 64;
    bool on = (ARBI_UINT8(seed) & 1) != 0;

    oled_ssd1306_set_pixel(&dev, x, y, on);
    bool readback = oled_ssd1306_get_pixel(&dev, x, y);
    PROP_ASSERT(readback == on);
}

PROPERTY(clear_zeroes_all, 5000)
{
    mock_reset();

    oled_ssd1306_t dev;
    oled_ssd1306_cfg_t cfg = {
        .addr   = OLED_SSD1306_ADDR_A,
        .width  = 128,
        .height = 64,
    };
    oled_ssd1306_err_t err = oled_ssd1306_init(&dev, &prop_i2c, &cfg,
                                                framebuf);
    if (err != OLED_SSD1306_OK) return;

    /* Set some random pixels */
    for (int i = 0; i < 10; i++) {
        uint8_t x = ARBI_UINT8(seed) % 128;
        uint8_t y = ARBI_UINT8(seed) % 64;
        oled_ssd1306_set_pixel(&dev, x, y, true);
    }

    oled_ssd1306_clear(&dev);

    /* Verify all pixels are off */
    for (int i = 0; i < (int)sizeof(framebuf); i++) {
        PROP_ASSERT(framebuf[i] == 0);
    }
}

TEST_MAIN("prop_oled_ssd1306")
    RUN(draw_no_crash);
    RUN(flush_writes_data);
    RUN(pixel_roundtrip);
    RUN(clear_zeroes_all);
TEST_MAIN_END()
