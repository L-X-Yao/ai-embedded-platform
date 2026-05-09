/*
 * prop_ili9341.c --- property-based tests for ili9341
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_ili9341.c ../ili9341.c -o prop_ili9341
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ili9341.h"
#include <string.h>

/* ---------- inline SPI mock ---------- */

static int     mock_op_count_prop;
static uint8_t mock_cs_state_prop;
static bool    mock_dc_state_prop;
static bool    mock_rst_state_prop;

static void prop_mock_reset(void) {
    mock_op_count_prop  = 0;
    mock_cs_state_prop  = 1;
    mock_dc_state_prop  = true;
    mock_rst_state_prop = true;
}

static void prop_cs_low(void *ctx)  { (void)ctx; mock_cs_state_prop = 0; }
static void prop_cs_high(void *ctx) { (void)ctx; mock_cs_state_prop = 1; }
static void prop_dc_set(void *ctx, bool level) { (void)ctx; mock_dc_state_prop = level; }
static void prop_rst_set(void *ctx, bool level) { (void)ctx; mock_rst_state_prop = level; }
static void prop_delay(void *ctx, uint32_t ms)  { (void)ctx; (void)ms; }
static uint8_t prop_xfer(void *ctx, uint8_t tx) {
    (void)ctx; (void)tx;
    mock_op_count_prop++;
    return 0xFF;
}

static const ili9341_spi_t prop_spi = {
    .cs_low   = prop_cs_low,
    .cs_high  = prop_cs_high,
    .xfer     = prop_xfer,
    .dc_set   = prop_dc_set,
    .rst_set  = prop_rst_set,
    .delay_ms = prop_delay,
    .ctx      = NULL,
};

/* Property: rgb565 is deterministic */
PROPERTY(rgb565_deterministic, 10000) {
    uint8_t r = ARBI_UINT8(seed);
    uint8_t g = ARBI_UINT8(seed);
    uint8_t b = ARBI_UINT8(seed);

    uint16_t c1 = ili9341_rgb565(r, g, b);
    uint16_t c2 = ili9341_rgb565(r, g, b);
    PROP_ASSERT_EQ(c1, c2);
}

/* Property: rgb565 packs correctly -- R5 G6 B5 */
PROPERTY(rgb565_packing, 10000) {
    uint8_t r = ARBI_UINT8(seed);
    uint8_t g = ARBI_UINT8(seed);
    uint8_t b = ARBI_UINT8(seed);

    uint16_t c = ili9341_rgb565(r, g, b);

    /* Extract components back */
    uint8_t r5 = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g6 = (uint8_t)((c >> 5) & 0x3F);
    uint8_t b5 = (uint8_t)(c & 0x1F);

    /* Should match truncated input */
    PROP_ASSERT_EQ(r5, r >> 3);
    PROP_ASSERT_EQ(g6, g >> 2);
    PROP_ASSERT_EQ(b5, b >> 3);
}

/* Property: rotation preserves area (width * height is always 240*320) */
PROPERTY(rotation_preserves_area, 1000) {
    prop_mock_reset();
    ili9341_t dev;
    memset(&dev, 0, sizeof(dev));
    ili9341_init(&dev, &prop_spi);

    ili9341_rotation_t rot = (ili9341_rotation_t)(ARBI_SIZE(seed, 3));
    ili9341_set_rotation(&dev, rot);

    uint32_t area = (uint32_t)dev.cur_width * (uint32_t)dev.cur_height;
    PROP_ASSERT_EQ((long long)area, (long long)(ILI9341_WIDTH * ILI9341_HEIGHT));
}

/* Property: rotation dimensions are valid combos */
PROPERTY(rotation_valid_dims, 1000) {
    prop_mock_reset();
    ili9341_t dev;
    memset(&dev, 0, sizeof(dev));
    ili9341_init(&dev, &prop_spi);

    ili9341_rotation_t rot = (ili9341_rotation_t)(ARBI_SIZE(seed, 3));
    ili9341_set_rotation(&dev, rot);

    /* Dimensions must be either 240x320 or 320x240 */
    bool is_portrait = (dev.cur_width == ILI9341_WIDTH && dev.cur_height == ILI9341_HEIGHT);
    bool is_landscape = (dev.cur_width == ILI9341_HEIGHT && dev.cur_height == ILI9341_WIDTH);
    PROP_ASSERT(is_portrait || is_landscape);
}

/* Property: set_window with valid coords does not crash */
PROPERTY(set_window_valid_no_crash, 5000) {
    prop_mock_reset();
    ili9341_t dev;
    memset(&dev, 0, sizeof(dev));
    ili9341_init(&dev, &prop_spi);

    ili9341_rotation_t rot = (ili9341_rotation_t)(ARBI_SIZE(seed, 3));
    ili9341_set_rotation(&dev, rot);

    uint16_t max_x = dev.cur_width - 1;
    uint16_t max_y = dev.cur_height - 1;

    uint16_t x0 = (uint16_t)ARBI_SIZE(seed, max_x);
    uint16_t y0 = (uint16_t)ARBI_SIZE(seed, max_y);
    uint16_t x1 = (uint16_t)(x0 + ARBI_SIZE(seed, max_x - x0));
    uint16_t y1 = (uint16_t)(y0 + ARBI_SIZE(seed, max_y - y0));

    ili9341_err_t rc = ili9341_set_window(&dev, x0, y0, x1, y1);
    PROP_ASSERT_EQ(rc, ILI9341_OK);
}

/* Property: init does not crash */
PROPERTY(init_no_crash, 1000) {
    prop_mock_reset();
    ili9341_t dev;
    memset(&dev, 0, sizeof(dev));
    ili9341_err_t rc = ili9341_init(&dev, &prop_spi);
    PROP_ASSERT_EQ(rc, ILI9341_OK);
    PROP_ASSERT(dev.cur_width > 0);
    PROP_ASSERT(dev.cur_height > 0);
}

TEST_MAIN("ili9341 properties") {
    RUN(rgb565_deterministic);
    RUN(rgb565_packing);
    RUN(rotation_preserves_area);
    RUN(rotation_valid_dims);
    RUN(set_window_valid_no_crash);
    RUN(init_no_crash);
}
TEST_MAIN_END()
