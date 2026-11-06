// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "../sk6812.h"
#include <string.h>

/* ---------- mock HAL ---------- */

static uint8_t mock_sent_data[1024];
static size_t  mock_sent_len;
static int     mock_send_count;
static bool    mock_send_fail;
static int     mock_delay_count;
static uint32_t mock_delay_us_total;

static void mock_reset_all(void)
{
    memset(mock_sent_data, 0, sizeof(mock_sent_data));
    mock_sent_len = 0;
    mock_send_count = 0;
    mock_send_fail = false;
    mock_delay_count = 0;
    mock_delay_us_total = 0;
}

static bool mock_send_data(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    mock_send_count++;

    if (mock_send_fail) return false;

    size_t copy = len < sizeof(mock_sent_data) ? len : sizeof(mock_sent_data);
    memcpy(mock_sent_data, data, copy);
    mock_sent_len = len;

    return true;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_delay_count++;
    mock_delay_us_total += us;
}

static const sk6812_hal_t mock_hal = {
    .send_data = mock_send_data,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));

    sk6812_err_t e = sk6812_init(&dev, &mock_hal, 8);
    ASSERT_EQ(e, SK6812_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_EQ(dev.num_leds, 8);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(sk6812_init(NULL, &mock_hal, 8), SK6812_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sk6812_init(&dev, NULL, 8), SK6812_ERR_PARAM);
}

TEST(test_init_zero_leds)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sk6812_init(&dev, &mock_hal, 0), SK6812_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));

    sk6812_hal_t bad = {
        .send_data = NULL,
        .delay_us  = mock_delay_us,
        .ctx       = NULL,
    };
    ASSERT_EQ(sk6812_init(&dev, &bad, 8), SK6812_ERR_PARAM);
}

TEST(test_set_pixel)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 4);

    sk6812_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));

    sk6812_err_t e = sk6812_set_pixel(&dev, buf, 0, 255, 128, 64, 32);
    ASSERT_EQ(e, SK6812_OK);

    /* GRBW order in struct */
    ASSERT_EQ(buf[0].r, 255);
    ASSERT_EQ(buf[0].g, 128);
    ASSERT_EQ(buf[0].b, 64);
    ASSERT_EQ(buf[0].w, 32);
}

TEST(test_set_pixel_last)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 4);

    sk6812_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));

    sk6812_err_t e = sk6812_set_pixel(&dev, buf, 3, 10, 20, 30, 40);
    ASSERT_EQ(e, SK6812_OK);
    ASSERT_EQ(buf[3].r, 10);
    ASSERT_EQ(buf[3].g, 20);
    ASSERT_EQ(buf[3].b, 30);
    ASSERT_EQ(buf[3].w, 40);
}

TEST(test_set_pixel_out_of_bounds)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 4);

    sk6812_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));

    ASSERT_EQ(sk6812_set_pixel(&dev, buf, 4, 255, 255, 255, 255),
              SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_set_pixel(&dev, buf, 100, 0, 0, 0, 0),
              SK6812_ERR_PARAM);
}

TEST(test_set_pixel_null)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 4);

    ASSERT_EQ(sk6812_set_pixel(NULL, NULL, 0, 0, 0, 0, 0), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_set_pixel(&dev, NULL, 0, 0, 0, 0, 0), SK6812_ERR_PARAM);
}

TEST(test_show)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));

    buf[0].g = 0x11;
    buf[0].r = 0x22;
    buf[0].b = 0x33;
    buf[0].w = 0x44;
    buf[1].g = 0x55;
    buf[1].r = 0x66;
    buf[1].b = 0x77;
    buf[1].w = 0x88;

    sk6812_err_t e = sk6812_show(&dev, buf);
    ASSERT_EQ(e, SK6812_OK);

    /* Sent data: 2 LEDs * 4 bytes = 8 bytes */
    ASSERT_EQ(mock_sent_len, 8);
    ASSERT_EQ(mock_send_count, 1);

    /* Verify GRBW wire order (struct layout matches) */
    ASSERT_EQ(mock_sent_data[0], 0x11); /* G */
    ASSERT_EQ(mock_sent_data[1], 0x22); /* R */
    ASSERT_EQ(mock_sent_data[2], 0x33); /* B */
    ASSERT_EQ(mock_sent_data[3], 0x44); /* W */
    ASSERT_EQ(mock_sent_data[4], 0x55); /* G */
    ASSERT_EQ(mock_sent_data[5], 0x66); /* R */
    ASSERT_EQ(mock_sent_data[6], 0x77); /* B */
    ASSERT_EQ(mock_sent_data[7], 0x88); /* W */

    /* Reset delay should have been called */
    ASSERT_TRUE(mock_delay_count >= 1);
    ASSERT_TRUE(mock_delay_us_total >= 100);
}

TEST(test_show_null)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    ASSERT_EQ(sk6812_show(NULL, NULL), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_show(&dev, NULL), SK6812_ERR_PARAM);
}

TEST(test_show_hal_failure)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));

    mock_send_fail = true;
    ASSERT_EQ(sk6812_show(&dev, buf), SK6812_ERR_HAL);
}

TEST(test_fill)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 3);

    sk6812_pixel_t buf[3];
    memset(buf, 0, sizeof(buf));

    sk6812_err_t e = sk6812_fill(&dev, buf, 100, 50, 25, 200);
    ASSERT_EQ(e, SK6812_OK);

    uint16_t i;
    for (i = 0; i < 3; i++) {
        ASSERT_EQ(buf[i].r, 100);
        ASSERT_EQ(buf[i].g, 50);
        ASSERT_EQ(buf[i].b, 25);
        ASSERT_EQ(buf[i].w, 200);
    }
}

TEST(test_fill_null)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 3);

    ASSERT_EQ(sk6812_fill(NULL, NULL, 0, 0, 0, 0), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_fill(&dev, NULL, 0, 0, 0, 0), SK6812_ERR_PARAM);
}

TEST(test_clear)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 3);

    sk6812_pixel_t buf[3];
    /* Fill with non-zero first */
    sk6812_fill(&dev, buf, 255, 255, 255, 255);

    sk6812_err_t e = sk6812_clear(&dev, buf);
    ASSERT_EQ(e, SK6812_OK);

    uint16_t i;
    for (i = 0; i < 3; i++) {
        ASSERT_EQ(buf[i].r, 0);
        ASSERT_EQ(buf[i].g, 0);
        ASSERT_EQ(buf[i].b, 0);
        ASSERT_EQ(buf[i].w, 0);
    }
}

TEST(test_clear_null)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 3);

    ASSERT_EQ(sk6812_clear(NULL, NULL), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_clear(&dev, NULL), SK6812_ERR_PARAM);
}

TEST(test_set_then_show)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));

    sk6812_set_pixel(&dev, buf, 0, 0xFF, 0x00, 0x00, 0x00); /* red */
    sk6812_set_pixel(&dev, buf, 1, 0x00, 0xFF, 0x00, 0x00); /* green */

    sk6812_show(&dev, buf);

    /* LED 0: R=0xFF → wire: G=0x00, R=0xFF, B=0x00, W=0x00 */
    ASSERT_EQ(mock_sent_data[0], 0x00); /* G */
    ASSERT_EQ(mock_sent_data[1], 0xFF); /* R */
    ASSERT_EQ(mock_sent_data[2], 0x00); /* B */
    ASSERT_EQ(mock_sent_data[3], 0x00); /* W */

    /* LED 1: G=0xFF → wire: G=0xFF, R=0x00, B=0x00, W=0x00 */
    ASSERT_EQ(mock_sent_data[4], 0xFF); /* G */
    ASSERT_EQ(mock_sent_data[5], 0x00); /* R */
    ASSERT_EQ(mock_sent_data[6], 0x00); /* B */
    ASSERT_EQ(mock_sent_data[7], 0x00); /* W */
}

TEST(test_fill_and_show)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    sk6812_fill(&dev, buf, 0xAA, 0xBB, 0xCC, 0xDD);
    sk6812_show(&dev, buf);

    /* Both pixels should be identical */
    ASSERT_EQ(mock_sent_data[0], 0xBB); /* G */
    ASSERT_EQ(mock_sent_data[1], 0xAA); /* R */
    ASSERT_EQ(mock_sent_data[2], 0xCC); /* B */
    ASSERT_EQ(mock_sent_data[3], 0xDD); /* W */
    ASSERT_EQ(mock_sent_data[4], 0xBB);
    ASSERT_EQ(mock_sent_data[5], 0xAA);
    ASSERT_EQ(mock_sent_data[6], 0xCC);
    ASSERT_EQ(mock_sent_data[7], 0xDD);
}

TEST(test_single_led)
{
    mock_reset_all();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 1);

    sk6812_pixel_t buf[1];
    sk6812_fill(&dev, buf, 10, 20, 30, 40);
    sk6812_show(&dev, buf);

    ASSERT_EQ(mock_sent_len, 4);
}

/* ---------- a device init did not accept ---------- */

static void assert_device_refused(sk6812_t *d)
{
    sk6812_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));
    ASSERT_EQ(sk6812_fill(d, buf, 1, 2, 3, 4), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_clear(d, buf), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_set_pixel(d, buf, 0, 1, 2, 3, 4), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_show(d, buf), SK6812_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static sk6812_t zeroed;
    assert_device_refused(&zeroed);
}

/* num_leds holds whatever the stack did, far more than the 4 pixels of the
 * buffer, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const sk6812_hal_t zeroed;
    sk6812_t d;

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(sk6812_init(&d, NULL, 4), SK6812_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(sk6812_init(&d, &zeroed, 4), SK6812_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(sk6812_init(&d, &mock_hal, 0), SK6812_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("sk6812")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_zero_leds);
    RUN(test_init_null_callbacks);
    RUN(test_set_pixel);
    RUN(test_set_pixel_last);
    RUN(test_set_pixel_out_of_bounds);
    RUN(test_set_pixel_null);
    RUN(test_show);
    RUN(test_show_null);
    RUN(test_show_hal_failure);
    RUN(test_fill);
    RUN(test_fill_null);
    RUN(test_clear);
    RUN(test_clear_null);
    RUN(test_set_then_show);
    RUN(test_fill_and_show);
    RUN(test_single_led);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
