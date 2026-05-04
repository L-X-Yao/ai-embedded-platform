// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ws2812.h"

static const ws2812_timing_t timing = {
    .period_ticks  = 59U,
    .t0h_ticks     = 16U,
    .t1h_ticks     = 31U,
    .reset_periods = 80U,
};

/* ---------- tests ---------- */

TEST(test_single_led_red)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t c = {255, 0, 0};  /* R=255, G=0, B=0 */
    ws2812_err_t err = ws2812_model_send_and_decode(&model, &timing, &c, 1);
    ASSERT_EQ(err, WS2812_OK);
    ASSERT_EQ(ws2812_model_get_led_count(&model), 1);
    ws2812_model_rgb_t led = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led.r, 255);
    ASSERT_EQ(led.g, 0);
    ASSERT_EQ(led.b, 0);
}

TEST(test_single_led_green)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t c = {0, 255, 0};  /* R=0, G=255, B=0 */
    ws2812_model_send_and_decode(&model, &timing, &c, 1);
    ws2812_model_rgb_t led = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led.r, 0);
    ASSERT_EQ(led.g, 255);
    ASSERT_EQ(led.b, 0);
}

TEST(test_single_led_blue)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t c = {0, 0, 255};
    ws2812_model_send_and_decode(&model, &timing, &c, 1);
    ws2812_model_rgb_t led = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led.r, 0);
    ASSERT_EQ(led.g, 0);
    ASSERT_EQ(led.b, 255);
}

TEST(test_multi_led_sequence)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t colors[3] = {
        {255,   0,   0},  /* LED 0: red */
        {  0, 255,   0},  /* LED 1: green */
        {  0,   0, 255},  /* LED 2: blue */
    };
    ws2812_model_send_and_decode(&model, &timing, colors, 3);
    ASSERT_EQ(ws2812_model_get_led_count(&model), 3);

    ws2812_model_rgb_t led0 = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led0.r, 255);
    ASSERT_EQ(led0.g, 0);
    ASSERT_EQ(led0.b, 0);

    ws2812_model_rgb_t led1 = ws2812_model_get_led(&model, 1);
    ASSERT_EQ(led1.r, 0);
    ASSERT_EQ(led1.g, 255);
    ASSERT_EQ(led1.b, 0);

    ws2812_model_rgb_t led2 = ws2812_model_get_led(&model, 2);
    ASSERT_EQ(led2.r, 0);
    ASSERT_EQ(led2.g, 0);
    ASSERT_EQ(led2.b, 255);
}

TEST(test_all_off)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t colors[4] = {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}};
    ws2812_model_send_and_decode(&model, &timing, colors, 4);
    ASSERT_EQ(ws2812_model_get_led_count(&model), 4);
    for (uint32_t i = 0; i < 4; i++) {
        ws2812_model_rgb_t led = ws2812_model_get_led(&model, i);
        ASSERT_EQ(led.r, 0);
        ASSERT_EQ(led.g, 0);
        ASSERT_EQ(led.b, 0);
    }
}

TEST(test_grb_byte_order)
{
    /*
     * The WS2812 sends data in GRB order. If the model decoded it
     * wrong (e.g. as RGB), red input would show up in the wrong channel.
     * We send R=0xAA, G=0x55, B=0xCC and verify exact decode.
     */
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t c = {0xAA, 0x55, 0xCC};
    ws2812_model_send_and_decode(&model, &timing, &c, 1);
    ws2812_model_rgb_t led = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led.r, 0xAA);
    ASSERT_EQ(led.g, 0x55);
    ASSERT_EQ(led.b, 0xCC);
}

TEST(test_all_white)
{
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t c = {255, 255, 255};
    ws2812_model_send_and_decode(&model, &timing, &c, 1);
    ws2812_model_rgb_t led = ws2812_model_get_led(&model, 0);
    ASSERT_EQ(led.r, 255);
    ASSERT_EQ(led.g, 255);
    ASSERT_EQ(led.b, 255);
}

TEST(test_mixed_values_roundtrip)
{
    /*
     * Send a range of non-trivial color values and verify the model
     * round-trips them correctly through the PWM encoding/decoding.
     */
    ws2812_model_t model;
    ws2812_model_init(&model, &timing);
    ws2812_color_t colors[5] = {
        {128,  64,  32},
        {  1, 254, 127},
        {200, 100,  50},
        { 33,  66,  99},
        {255, 128,   0},
    };
    ws2812_model_send_and_decode(&model, &timing, colors, 5);
    ASSERT_EQ(ws2812_model_get_led_count(&model), 5);

    for (uint32_t i = 0; i < 5; i++) {
        ws2812_model_rgb_t led = ws2812_model_get_led(&model, i);
        ASSERT_EQ(led.r, colors[i].r);
        ASSERT_EQ(led.g, colors[i].g);
        ASSERT_EQ(led.b, colors[i].b);
    }
}

TEST_MAIN("ws2812_hw_model")
    RUN(test_single_led_red);
    RUN(test_single_led_green);
    RUN(test_single_led_blue);
    RUN(test_multi_led_sequence);
    RUN(test_all_off);
    RUN(test_grb_byte_order);
    RUN(test_all_white);
    RUN(test_mixed_values_roundtrip);
TEST_MAIN_END()
