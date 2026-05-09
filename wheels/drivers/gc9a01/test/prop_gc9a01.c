// SPDX-License-Identifier: MIT
/*
 * prop_gc9a01.c — property-based tests for gc9a01
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_gc9a01.c ../gc9a01.c -o prop_gc9a01
 */

#include "test_harness.h"
#include "property_test.h"
#include "gc9a01.h"
#include <string.h>

/* ---------- mock SPI ---------- */

static int mock_xfer_count;

static void mock_reset(void)
{
    mock_xfer_count = 0;
}

static void mock_cs_low(void *ctx)   { (void)ctx; }
static void mock_cs_high(void *ctx)  { (void)ctx; }
static void mock_dc_cmd(void *ctx)   { (void)ctx; }
static void mock_dc_data(void *ctx)  { (void)ctx; }
static void mock_rst_low(void *ctx)  { (void)ctx; }
static void mock_rst_high(void *ctx) { (void)ctx; }

static void mock_delay_ms(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    mock_xfer_count++;
    return 0xFF;
}

static const gc9a01_spi_t prop_spi = {
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

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    gc9a01_t dev;
    gc9a01_init(&dev, &prop_spi);
    /* Must not crash */
}

PROPERTY(rotation_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    gc9a01_t dev;
    gc9a01_init(&dev, &prop_spi);

    uint8_t rot = ARBI_UINT8(seed);
    gc9a01_set_rotation(&dev, rot);
    /* Must not crash even with invalid rotation values */
}

PROPERTY(rotation_valid_preserves_dims, 5000)
{
    mock_reset();

    gc9a01_t dev;
    gc9a01_err_t err = gc9a01_init(&dev, &prop_spi);
    if (err != GC9A01_OK) return;

    uint8_t rot = ARBI_UINT8(seed) % 4;
    err = gc9a01_set_rotation(&dev, rot);
    PROP_ASSERT(err == GC9A01_OK);
    /* GC9A01 is 240x240 round display, dimensions stay the same */
    PROP_ASSERT_EQ(dev.width, 240);
    PROP_ASSERT_EQ(dev.height, 240);
    PROP_ASSERT_EQ(dev.rotation, rot);
}

PROPERTY(set_window_valid_no_crash, 5000)
{
    mock_reset();

    gc9a01_t dev;
    gc9a01_err_t err = gc9a01_init(&dev, &prop_spi);
    if (err != GC9A01_OK) return;

    /* Generate valid coordinates within 240x240 */
    uint16_t x0 = ARBI_UINT16(seed) % 240;
    uint16_t y0 = ARBI_UINT16(seed) % 240;
    uint16_t x1 = x0 + (ARBI_UINT16(seed) % (240 - x0));
    uint16_t y1 = y0 + (ARBI_UINT16(seed) % (240 - y0));

    err = gc9a01_set_window(&dev, x0, y0, x1, y1);
    PROP_ASSERT(err == GC9A01_OK);
}

PROPERTY(rgb565_deterministic, 10000)
{
    uint8_t r = ARBI_UINT8(seed);
    uint8_t g = ARBI_UINT8(seed);
    uint8_t b = ARBI_UINT8(seed);

    uint16_t c1 = gc9a01_rgb565(r, g, b);
    uint16_t c2 = gc9a01_rgb565(r, g, b);
    PROP_ASSERT_EQ(c1, c2);

    /* Verify component extraction is consistent:
     * R uses top 5 bits of r, G uses top 6 bits of g, B uses top 5 bits of b */
    uint16_t expected = (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                                   ((uint16_t)(g & 0xFC) << 3) |
                                   ((uint16_t)(b >> 3)));
    PROP_ASSERT_EQ(c1, expected);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    gc9a01_t dev;
    gc9a01_init(&dev, &prop_spi);
    gc9a01_init(&dev, &prop_spi);
    /* Must not crash */
}

PROPERTY(sleep_toggle_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    gc9a01_t dev;
    gc9a01_err_t err = gc9a01_init(&dev, &prop_spi);
    if (err != GC9A01_OK) return;

    bool enable = (ARBI_UINT8(seed) & 1) != 0;
    err = gc9a01_sleep(&dev, enable);
    PROP_ASSERT(err == GC9A01_OK);
}

TEST_MAIN("gc9a01 properties")
    RUN(init_no_crash);
    RUN(rotation_no_crash);
    RUN(rotation_valid_preserves_dims);
    RUN(set_window_valid_no_crash);
    RUN(rgb565_deterministic);
    RUN(double_init_safe);
    RUN(sleep_toggle_no_crash);
TEST_MAIN_END()
