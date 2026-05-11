// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "sk6812.h"

/* ---------- mock HAL ---------- */

static uint8_t  sent_data[1024];
static uint32_t sent_len;
static int      send_count;
static int      send_fail_after;

static void mock_reset(void)
{
    memset(sent_data, 0, sizeof(sent_data));
    sent_len = 0;
    send_count = 0;
    send_fail_after = 0;
}

static bool mock_send_data(void *ctx, const uint8_t *data, uint32_t len)
{
    (void)ctx;
    send_count++;
    if (send_fail_after > 0 && send_count > send_fail_after) return false;
    if (len <= sizeof(sent_data)) {
        memcpy(sent_data, data, len);
        sent_len = len;
    }
    return true;
}

static bool mock_send_data_always_fail(void *ctx, const uint8_t *data, uint32_t len)
{
    (void)ctx; (void)data; (void)len;
    return false;
}

static const sk6812_hal_t mock_hal = {
    .send_data = mock_send_data,
    .ctx       = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));

    sk6812_err_t e = sk6812_init(&dev, &mock_hal, 10);
    ASSERT_EQ(e, SK6812_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_EQ(dev.num_leds, 10);
}

TEST(test_init_null_params)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));

    ASSERT_EQ(sk6812_init(NULL, &mock_hal, 10), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_init(&dev, NULL, 10), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_init(&dev, &mock_hal, 0), SK6812_ERR_PARAM);
}

TEST(test_init_null_callback)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));

    sk6812_hal_t bad_hal = { .send_data = NULL, .ctx = NULL };
    ASSERT_EQ(sk6812_init(&dev, &bad_hal, 10), SK6812_ERR_PARAM);
}

TEST(test_set_pixel)
{
    sk6812_pixel_t buf[3];
    memset(buf, 0, sizeof(buf));

    sk6812_err_t e = sk6812_set_pixel(buf, 3, 1, 0xFF, 0x80, 0x40, 0x20);
    ASSERT_EQ(e, SK6812_OK);
    ASSERT_EQ(buf[1].r, 0xFF);
    ASSERT_EQ(buf[1].g, 0x80);
    ASSERT_EQ(buf[1].b, 0x40);
    ASSERT_EQ(buf[1].w, 0x20);
}

TEST(test_set_pixel_out_of_bounds)
{
    sk6812_pixel_t buf[3];
    memset(buf, 0, sizeof(buf));

    ASSERT_EQ(sk6812_set_pixel(buf, 3, 3, 0, 0, 0, 0), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_set_pixel(buf, 3, 100, 0, 0, 0, 0), SK6812_ERR_PARAM);
}

TEST(test_set_pixel_null)
{
    ASSERT_EQ(sk6812_set_pixel(NULL, 3, 0, 0, 0, 0, 0), SK6812_ERR_PARAM);
}

TEST(test_show)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));
    buf[0].g = 0x11;
    buf[0].r = 0x22;
    buf[0].b = 0x33;
    buf[0].w = 0x44;
    buf[1].g = 0xAA;
    buf[1].r = 0xBB;
    buf[1].b = 0xCC;
    buf[1].w = 0xDD;

    sk6812_err_t e = sk6812_show(&dev, buf, 2);
    ASSERT_EQ(e, SK6812_OK);
    ASSERT_EQ(sent_len, 8U); /* 2 * 4 bytes */

    /* Wire order: G,R,B,W for each pixel */
    ASSERT_EQ(sent_data[0], 0x11); /* G */
    ASSERT_EQ(sent_data[1], 0x22); /* R */
    ASSERT_EQ(sent_data[2], 0x33); /* B */
    ASSERT_EQ(sent_data[3], 0x44); /* W */
    ASSERT_EQ(sent_data[4], 0xAA);
    ASSERT_EQ(sent_data[5], 0xBB);
    ASSERT_EQ(sent_data[6], 0xCC);
    ASSERT_EQ(sent_data[7], 0xDD);
}

TEST(test_show_null)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));

    ASSERT_EQ(sk6812_show(NULL, buf, 2), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_show(&dev, NULL, 2), SK6812_ERR_PARAM);
    ASSERT_EQ(sk6812_show(&dev, buf, 0), SK6812_ERR_PARAM);
}

TEST(test_show_hal_error)
{
    mock_reset();
    sk6812_hal_t fail_hal = { .send_data = mock_send_data_always_fail, .ctx = NULL };
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &fail_hal, 2);

    sk6812_pixel_t buf[2];
    memset(buf, 0, sizeof(buf));
    sk6812_err_t e = sk6812_show(&dev, buf, 2);
    ASSERT_EQ(e, SK6812_ERR_HAL);
}

TEST(test_fill)
{
    sk6812_pixel_t buf[4];
    memset(buf, 0, sizeof(buf));

    sk6812_err_t e = sk6812_fill(buf, 4, 0xAA, 0xBB, 0xCC, 0xDD);
    ASSERT_EQ(e, SK6812_OK);

    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(buf[i].r, 0xAA);
        ASSERT_EQ(buf[i].g, 0xBB);
        ASSERT_EQ(buf[i].b, 0xCC);
        ASSERT_EQ(buf[i].w, 0xDD);
    }
}

TEST(test_fill_null)
{
    ASSERT_EQ(sk6812_fill(NULL, 4, 0, 0, 0, 0), SK6812_ERR_PARAM);

    sk6812_pixel_t buf[1];
    ASSERT_EQ(sk6812_fill(buf, 0, 0, 0, 0, 0), SK6812_ERR_PARAM);
}

TEST(test_clear)
{
    sk6812_pixel_t buf[3];
    /* Fill with non-zero */
    memset(buf, 0xFF, sizeof(buf));

    sk6812_err_t e = sk6812_clear(buf, 3);
    ASSERT_EQ(e, SK6812_OK);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(buf[i].g, 0);
        ASSERT_EQ(buf[i].r, 0);
        ASSERT_EQ(buf[i].b, 0);
        ASSERT_EQ(buf[i].w, 0);
    }
}

TEST(test_clear_null)
{
    ASSERT_EQ(sk6812_clear(NULL, 3), SK6812_ERR_PARAM);

    sk6812_pixel_t buf[1];
    ASSERT_EQ(sk6812_clear(buf, 0), SK6812_ERR_PARAM);
}

TEST(test_grbw_wire_order)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 1);

    sk6812_pixel_t buf[1];
    /* Set via set_pixel: r=0xAA, g=0xBB, b=0xCC, w=0xDD */
    sk6812_set_pixel(buf, 1, 0, 0xAA, 0xBB, 0xCC, 0xDD);

    sk6812_show(&dev, buf, 1);

    /* Wire order: G, R, B, W */
    ASSERT_EQ(sent_data[0], 0xBB); /* G */
    ASSERT_EQ(sent_data[1], 0xAA); /* R */
    ASSERT_EQ(sent_data[2], 0xCC); /* B */
    ASSERT_EQ(sent_data[3], 0xDD); /* W */
}

TEST(test_single_pixel_all_white)
{
    mock_reset();
    sk6812_t dev;
    memset(&dev, 0, sizeof(dev));
    sk6812_init(&dev, &mock_hal, 1);

    sk6812_pixel_t buf[1];
    sk6812_set_pixel(buf, 1, 0, 0, 0, 0, 0xFF);

    sk6812_show(&dev, buf, 1);
    ASSERT_EQ(sent_data[0], 0x00); /* G */
    ASSERT_EQ(sent_data[1], 0x00); /* R */
    ASSERT_EQ(sent_data[2], 0x00); /* B */
    ASSERT_EQ(sent_data[3], 0xFF); /* W */
}

TEST_MAIN("sk6812")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_init_null_callback);
    RUN(test_set_pixel);
    RUN(test_set_pixel_out_of_bounds);
    RUN(test_set_pixel_null);
    RUN(test_show);
    RUN(test_show_null);
    RUN(test_show_hal_error);
    RUN(test_fill);
    RUN(test_fill_null);
    RUN(test_clear);
    RUN(test_clear_null);
    RUN(test_grbw_wire_order);
    RUN(test_single_pixel_all_white);
TEST_MAIN_END()