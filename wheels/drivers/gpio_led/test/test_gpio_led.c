// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "gpio_led.h"

/* ---------- mock HAL ---------- */

#define MAX_CALLS 512

static uint8_t mock_levels[MAX_CALLS];
static int     mock_idx;
static uint8_t mock_current;

static void mock_reset(void)
{
    memset(mock_levels, 0, sizeof(mock_levels));
    mock_idx = 0;
    mock_current = 0;
}

static void mock_set(void *ctx, uint8_t level)
{
    (void)ctx;
    mock_current = level;
    if (mock_idx < MAX_CALLS) {
        mock_levels[mock_idx++] = level;
    }
}

static const gpio_led_hal_t mock_hal = {
    .set = mock_set,
    .ctx = NULL,
};

/* ---------- test helpers ---------- */
/* ---------- tests ---------- */

TEST(test_init)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
    ASSERT_TRUE(mock_current == 0);
    ASSERT_TRUE(mock_idx == 1);
}

TEST(test_on_off)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_on(&led);
    ASSERT_TRUE(mock_current == 255);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_ON);

    gpio_led_off(&led);
    ASSERT_TRUE(mock_current == 0);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
}

TEST(test_blink_basic)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 0);

    ASSERT_TRUE(mock_current == 255);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BLINK);

    /* tick 50ms: still in ON phase */
    gpio_led_tick(&led, 50);
    ASSERT_TRUE(mock_current == 255);

    /* tick 50ms more: transition to OFF at 100ms */
    gpio_led_tick(&led, 50);
    ASSERT_TRUE(mock_current == 0);

    /* tick 100ms: back to ON at 200ms */
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 255);
}

TEST(test_blink_asymmetric)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 200, 50, 0);

    ASSERT_TRUE(mock_current == 255);

    gpio_led_tick(&led, 200);
    ASSERT_TRUE(mock_current == 0);

    gpio_led_tick(&led, 50);
    ASSERT_TRUE(mock_current == 255);
}

TEST(test_blink_repeat)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 2);

    /* cycle 1: ON→OFF */
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 255);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BLINK);

    /* cycle 2: ON→OFF → should stop */
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
}

TEST(test_blink_single)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 50, 50, 1);

    gpio_led_tick(&led, 50);
    ASSERT_TRUE(mock_current == 0);
    gpio_led_tick(&led, 50);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
}

TEST(test_pulse)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_pulse(&led, 200);

    ASSERT_TRUE(mock_current == 255);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_PULSE);

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 255);

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);

    /* further ticks do nothing */
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);
}

TEST(test_breathe_ramp)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1000);

    ASSERT_TRUE(mock_current == 0);

    /* at 250ms: 50% through first half → ~127 */
    gpio_led_tick(&led, 250);
    ASSERT_TRUE(mock_current > 120 && mock_current < 135);

    /* at 500ms: peak */
    gpio_led_tick(&led, 250);
    ASSERT_TRUE(mock_current > 240);

    /* at 750ms: ramp down to ~127 */
    gpio_led_tick(&led, 250);
    ASSERT_TRUE(mock_current > 120 && mock_current < 135);

    /* at 1000ms: back to 0, cycle repeats */
    gpio_led_tick(&led, 250);
    ASSERT_TRUE(mock_current < 10);
}

TEST(test_breathe_continuous)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 200);

    /* run 3 full cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        gpio_led_tick(&led, 100);
        ASSERT_TRUE(mock_current > 200);
        gpio_led_tick(&led, 100);
    }
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BREATHE);
}

TEST(test_mode_switch)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_blink(&led, 100, 100, 0);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BLINK);

    gpio_led_on(&led);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_ON);
    ASSERT_TRUE(mock_current == 255);

    /* tick should do nothing in ON mode */
    gpio_led_tick(&led, 200);
    ASSERT_TRUE(mock_current == 255);

    gpio_led_breathe(&led, 500);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BREATHE);
}

TEST(test_no_redundant_calls)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_on(&led);
    int calls_after_on = mock_idx;

    /* tick in ON mode should not call set again */
    gpio_led_tick(&led, 100);
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_idx == calls_after_on);

    gpio_led_off(&led);
    int calls_after_off = mock_idx;

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_idx == calls_after_off);
}

TEST(test_null_safety)
{
    gpio_led_init(NULL, &mock_hal);
    gpio_led_on(NULL);
    gpio_led_off(NULL);
    gpio_led_blink(NULL, 100, 100, 0);
    gpio_led_breathe(NULL, 500);
    gpio_led_pulse(NULL, 200);
    gpio_led_tick(NULL, 10);
    ASSERT_TRUE(gpio_led_get_mode(NULL) == GPIO_LED_OFF);
    /* no crash = pass */
    ASSERT_TRUE(1);
}

TEST(test_zero_dt)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 0);

    int calls_before = mock_idx;
    gpio_led_tick(&led, 0);
    ASSERT_TRUE(mock_idx == calls_before);
}

TEST(test_custom_brightness)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    led.brightness = 128;

    gpio_led_on(&led);
    ASSERT_TRUE(mock_current == 128);

    gpio_led_blink(&led, 100, 100, 0);
    ASSERT_TRUE(mock_current == 128);

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 128);
}

TEST(test_reinit_resets)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_on(&led);
    ASSERT_TRUE(mock_current == 255);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_ON);

    gpio_led_init(&led, &mock_hal);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
    ASSERT_TRUE(mock_current == 0);
}

TEST(test_blink_zero_on)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 0, 100, 0);

    gpio_led_tick(&led, 1);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BLINK);

    gpio_led_tick(&led, 1);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BLINK);
}

TEST(test_pulse_zero_on)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_pulse(&led, 0);

    gpio_led_tick(&led, 1);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_OFF);
    ASSERT_TRUE(mock_current == 0);
}

TEST(test_breathe_period_one)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1);

    /* period=1, half=0→1: elapsed wraps to 0 every tick */
    gpio_led_tick(&led, 1);
    ASSERT_TRUE(mock_current == 0);
    gpio_led_tick(&led, 1);
    ASSERT_TRUE(mock_current == 0);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BREATHE);
}

TEST(test_breathe_period_zero)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 0);

    int calls_before = mock_idx;
    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_idx == calls_before);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BREATHE);
}

TEST(test_blink_zero_off)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 0, 0);

    ASSERT_TRUE(mock_current == 255);

    gpio_led_tick(&led, 100);
    ASSERT_TRUE(mock_current == 0);

    /* off_ms=0 clamped to 1: should return to ON after 1ms */
    gpio_led_tick(&led, 1);
    ASSERT_TRUE(mock_current == 255);
}

TEST(test_breathe_large_dt)
{
    gpio_led_t led;
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1000);

    /* tick with dt larger than period — should wrap correctly */
    gpio_led_tick(&led, 1500);
    /* elapsed = 1500 - 1000 = 500. half=500. elapsed(500) < half(500)? No.
       level = (1000-500)*255/500 = 255. */
    ASSERT_TRUE(mock_current > 240);
    ASSERT_TRUE(gpio_led_get_mode(&led) == GPIO_LED_BREATHE);
}

TEST_MAIN("gpio_led")
    RUN(test_init);
    RUN(test_on_off);
    RUN(test_blink_basic);
    RUN(test_blink_asymmetric);
    RUN(test_blink_repeat);
    RUN(test_blink_single);
    RUN(test_pulse);
    RUN(test_breathe_ramp);
    RUN(test_breathe_continuous);
    RUN(test_mode_switch);
    RUN(test_no_redundant_calls);
    RUN(test_null_safety);
    RUN(test_zero_dt);
    RUN(test_custom_brightness);
    RUN(test_reinit_resets);
    RUN(test_blink_zero_on);
    RUN(test_pulse_zero_on);
    RUN(test_breathe_period_one);
    RUN(test_breathe_period_zero);
    RUN(test_blink_zero_off);
    RUN(test_breathe_large_dt);
TEST_MAIN_END()

