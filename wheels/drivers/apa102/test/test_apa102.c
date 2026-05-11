// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "apa102.h"

/* ---------- mock SPI ---------- */

static int     mock_fail_at;
static int     mock_op_count;
static uint8_t mock_cs_state;

static uint8_t xfer_log[1024];
static int     xfer_log_idx;

static void mock_reset(void)
{
    mock_fail_at   = -1;
    mock_op_count  = 0;
    mock_cs_state  = 1;
    xfer_log_idx   = 0;
    memset(xfer_log, 0, sizeof(xfer_log));
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;
    return 0xFF;
}

static const apa102_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    apa102_t dev;
    ASSERT_EQ(apa102_init(&dev, &mock_spi, 10), APA102_OK);
    ASSERT_EQ(dev.num_leds, 10);
}

TEST(test_init_zero_leds)
{
    mock_reset();
    apa102_t dev;
    ASSERT_EQ(apa102_init(&dev, &mock_spi, 0), APA102_ERR_PARAM);
}

TEST(test_null_safety)
{
    apa102_t dev;
    ASSERT_EQ(apa102_init(NULL, &mock_spi, 10), APA102_ERR_PARAM);
    ASSERT_EQ(apa102_init(&dev, NULL, 10), APA102_ERR_PARAM);

    apa102_pixel_t buf[4];
    ASSERT_EQ(apa102_set_pixel(NULL, 4, 0, 255, 0, 0, 31), APA102_ERR_PARAM);
    ASSERT_EQ(apa102_show(NULL, buf), APA102_ERR_PARAM);

    mock_reset();
    apa102_init(&dev, &mock_spi, 4);
    ASSERT_EQ(apa102_show(&dev, NULL), APA102_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    apa102_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    apa102_t zero_dev;
    ASSERT_TRUE(apa102_init(&zero_dev, &zero_hal, 10) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)APA102_ERR_PARAM);
    ASSERT_EQ(-2, (int)APA102_ERR_SPI);
    ASSERT_EQ(-3, (int)APA102_ERR_ID);
    ASSERT_EQ(-4, (int)APA102_ERR_TIMEOUT);
}

TEST(test_set_pixel)
{
    apa102_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));

    ASSERT_EQ(apa102_set_pixel(buf, 4, 0, 255, 128, 64, 16), APA102_OK);
    ASSERT_EQ(buf[0].r, 255);
    ASSERT_EQ(buf[0].g, 128);
    ASSERT_EQ(buf[0].b, 64);
    ASSERT_EQ(buf[0].brightness, 16);
}

TEST(test_set_pixel_out_of_bounds)
{
    apa102_pixel_t buf[4];
    ASSERT_EQ(apa102_set_pixel(buf, 4, 4, 255, 0, 0, 31), APA102_ERR_PARAM);
    ASSERT_EQ(apa102_set_pixel(buf, 4, 100, 255, 0, 0, 31), APA102_ERR_PARAM);
}

TEST(test_set_pixel_clamps_brightness)
{
    apa102_pixel_t buf[1];
    memset(buf, 0, sizeof(buf));
    /* Brightness 50 should be clamped to 31 */
    apa102_set_pixel(buf, 1, 0, 0, 0, 0, 50);
    ASSERT_EQ(buf[0].brightness, 31);
}

TEST(test_clear)
{
    apa102_pixel_t buf[4];
    buf[0].r = 255; buf[0].brightness = 31;
    buf[1].g = 128;
    apa102_clear(buf, 4);
    ASSERT_EQ(buf[0].r, 0);
    ASSERT_EQ(buf[0].brightness, 0);
    ASSERT_EQ(buf[1].g, 0);
}

TEST(test_clear_null)
{
    apa102_clear(NULL, 10);
    ASSERT_TRUE(1); /* must not crash */
}

TEST(test_show_start_frame)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 1);

    apa102_pixel_t buf[1] = {{0, 0, 0, 0}};
    apa102_show(&dev, buf);

    /* First 4 bytes should be 0x00 (start frame) */
    ASSERT_EQ(xfer_log[0], 0x00);
    ASSERT_EQ(xfer_log[1], 0x00);
    ASSERT_EQ(xfer_log[2], 0x00);
    ASSERT_EQ(xfer_log[3], 0x00);
}

TEST(test_show_led_frame_format)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 1);

    apa102_pixel_t buf[1];
    buf[0].r = 0xAA;
    buf[0].g = 0xBB;
    buf[0].b = 0xCC;
    buf[0].brightness = 15;
    apa102_show(&dev, buf);

    /* After 4-byte start frame, LED frame at index 4: */
    ASSERT_EQ_HEX(xfer_log[4], 0xE0 | 15);  /* 0xE0 | brightness */
    ASSERT_EQ_HEX(xfer_log[5], 0xCC);        /* blue */
    ASSERT_EQ_HEX(xfer_log[6], 0xBB);        /* green */
    ASSERT_EQ_HEX(xfer_log[7], 0xAA);        /* red */
}

TEST(test_show_end_frame)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 1);

    apa102_pixel_t buf[1] = {{0, 0, 0, 0}};
    apa102_show(&dev, buf);

    /* Total: 4 start + 4 LED + 4 end (min 4 for <=64 LEDs) = 12 */
    ASSERT_EQ(xfer_log_idx, 12);
    /* End frame bytes should be 0xFF */
    ASSERT_EQ_HEX(xfer_log[8], 0xFF);
    ASSERT_EQ_HEX(xfer_log[9], 0xFF);
    ASSERT_EQ_HEX(xfer_log[10], 0xFF);
    ASSERT_EQ_HEX(xfer_log[11], 0xFF);
}

TEST(test_show_multiple_leds)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 3);

    apa102_pixel_t buf[3];
    apa102_set_pixel(buf, 3, 0, 255, 0, 0, 31);
    apa102_set_pixel(buf, 3, 1, 0, 255, 0, 20);
    apa102_set_pixel(buf, 3, 2, 0, 0, 255, 10);
    apa102_show(&dev, buf);

    /* 4 start + 3*4 LED + 4 end = 20 */
    ASSERT_EQ(xfer_log_idx, 20);

    /* LED 0: red */
    ASSERT_EQ_HEX(xfer_log[4], 0xE0 | 31);
    ASSERT_EQ(xfer_log[5], 0);     /* blue */
    ASSERT_EQ(xfer_log[6], 0);     /* green */
    ASSERT_EQ(xfer_log[7], 255);   /* red */

    /* LED 1: green */
    ASSERT_EQ_HEX(xfer_log[8], 0xE0 | 20);
    ASSERT_EQ(xfer_log[9], 0);     /* blue */
    ASSERT_EQ(xfer_log[10], 255);  /* green */
    ASSERT_EQ(xfer_log[11], 0);    /* red */

    /* LED 2: blue */
    ASSERT_EQ_HEX(xfer_log[12], 0xE0 | 10);
    ASSERT_EQ(xfer_log[13], 255);  /* blue */
    ASSERT_EQ(xfer_log[14], 0);    /* green */
    ASSERT_EQ(xfer_log[15], 0);    /* red */
}

TEST(test_show_cs_asserted)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 1);

    apa102_pixel_t buf[1] = {{0, 0, 0, 0}};
    apa102_show(&dev, buf);

    /* After show, CS should be deasserted (high) */
    ASSERT_EQ(mock_cs_state, 1);
}

TEST(test_show_brightness_clamped)
{
    mock_reset();
    apa102_t dev;
    apa102_init(&dev, &mock_spi, 1);

    apa102_pixel_t buf[1];
    buf[0].r = 0;
    buf[0].g = 0;
    buf[0].b = 0;
    buf[0].brightness = 50;  /* >31, should be clamped */
    apa102_show(&dev, buf);

    /* LED frame brightness should be clamped to 31 */
    ASSERT_EQ_HEX(xfer_log[4], 0xE0 | 31);
}

TEST_MAIN("apa102")
    RUN(test_init_ok);
    RUN(test_init_zero_leds);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_set_pixel);
    RUN(test_set_pixel_out_of_bounds);
    RUN(test_set_pixel_clamps_brightness);
    RUN(test_clear);
    RUN(test_clear_null);
    RUN(test_show_start_frame);
    RUN(test_show_led_frame_format);
    RUN(test_show_end_frame);
    RUN(test_show_multiple_leds);
    RUN(test_show_cs_asserted);
    RUN(test_show_brightness_clamped);
TEST_MAIN_END()
