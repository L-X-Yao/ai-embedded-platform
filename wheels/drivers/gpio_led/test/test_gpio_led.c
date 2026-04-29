#include <stdio.h>
#include <string.h>
#include "../gpio_led.h"

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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

/* ---------- tests ---------- */

static void test_init(void)
{
    gpio_led_t led;

    printf("test_init\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "mode OFF after init");
    ASSERT(mock_current == 0, "LED off after init");
    ASSERT(mock_idx == 1, "one set call on init");
}

static void test_on_off(void)
{
    gpio_led_t led;

    printf("test_on_off\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_on(&led);
    ASSERT(mock_current == 255, "on → 255");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_ON, "mode ON");

    gpio_led_off(&led);
    ASSERT(mock_current == 0, "off → 0");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "mode OFF");
}

static void test_blink_basic(void)
{
    gpio_led_t led;

    printf("test_blink_basic\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 0);

    ASSERT(mock_current == 255, "blink starts ON");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK, "mode BLINK");

    /* tick 50ms: still in ON phase */
    gpio_led_tick(&led, 50);
    ASSERT(mock_current == 255, "still ON at 50ms");

    /* tick 50ms more: transition to OFF at 100ms */
    gpio_led_tick(&led, 50);
    ASSERT(mock_current == 0, "OFF at 100ms");

    /* tick 100ms: back to ON at 200ms */
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 255, "ON at 200ms");
}

static void test_blink_asymmetric(void)
{
    gpio_led_t led;

    printf("test_blink_asymmetric\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 200, 50, 0);

    ASSERT(mock_current == 255, "starts ON");

    gpio_led_tick(&led, 200);
    ASSERT(mock_current == 0, "OFF at 200ms");

    gpio_led_tick(&led, 50);
    ASSERT(mock_current == 255, "ON at 250ms");
}

static void test_blink_repeat(void)
{
    gpio_led_t led;

    printf("test_blink_repeat\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 2);

    /* cycle 1: ON→OFF */
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "cycle 1 OFF");
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 255, "cycle 2 ON");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK, "still blinking");

    /* cycle 2: ON→OFF → should stop */
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "cycle 2 OFF");
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "stopped after 2 repeats");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "mode OFF after repeat");
}

static void test_blink_single(void)
{
    gpio_led_t led;

    printf("test_blink_single\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 50, 50, 1);

    gpio_led_tick(&led, 50);
    ASSERT(mock_current == 0, "OFF phase");
    gpio_led_tick(&led, 50);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "stopped after 1");
}

static void test_pulse(void)
{
    gpio_led_t led;

    printf("test_pulse\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_pulse(&led, 200);

    ASSERT(mock_current == 255, "pulse starts ON");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_PULSE, "mode PULSE");

    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 255, "still ON at 100ms");

    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "OFF at 200ms");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "mode OFF after pulse");

    /* further ticks do nothing */
    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "stays OFF");
}

static void test_breathe_ramp(void)
{
    gpio_led_t led;

    printf("test_breathe_ramp\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1000);

    ASSERT(mock_current == 0, "breathe starts at 0");

    /* at 250ms: 50% through first half → ~127 */
    gpio_led_tick(&led, 250);
    ASSERT(mock_current > 120 && mock_current < 135, "ramp up at 250ms");

    /* at 500ms: peak */
    gpio_led_tick(&led, 250);
    ASSERT(mock_current > 240, "peak at 500ms");

    /* at 750ms: ramp down to ~127 */
    gpio_led_tick(&led, 250);
    ASSERT(mock_current > 120 && mock_current < 135, "ramp down at 750ms");

    /* at 1000ms: back to 0, cycle repeats */
    gpio_led_tick(&led, 250);
    ASSERT(mock_current < 10, "back to 0 at 1000ms");
}

static void test_breathe_continuous(void)
{
    gpio_led_t led;

    printf("test_breathe_continuous\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 200);

    /* run 3 full cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        gpio_led_tick(&led, 100);
        ASSERT(mock_current > 200, "peak each cycle");
        gpio_led_tick(&led, 100);
    }
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BREATHE, "still breathing");
}

static void test_mode_switch(void)
{
    gpio_led_t led;

    printf("test_mode_switch\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_blink(&led, 100, 100, 0);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK, "blink");

    gpio_led_on(&led);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_ON, "switch to on");
    ASSERT(mock_current == 255, "level 255");

    /* tick should do nothing in ON mode */
    gpio_led_tick(&led, 200);
    ASSERT(mock_current == 255, "stays on after tick");

    gpio_led_breathe(&led, 500);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BREATHE, "switch to breathe");
}

static void test_no_redundant_calls(void)
{
    gpio_led_t led;

    printf("test_no_redundant_calls\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);

    gpio_led_on(&led);
    int calls_after_on = mock_idx;

    /* tick in ON mode should not call set again */
    gpio_led_tick(&led, 100);
    gpio_led_tick(&led, 100);
    ASSERT(mock_idx == calls_after_on, "no redundant set calls in ON");

    gpio_led_off(&led);
    int calls_after_off = mock_idx;

    gpio_led_tick(&led, 100);
    ASSERT(mock_idx == calls_after_off, "no redundant set calls in OFF");
}

static void test_null_safety(void)
{
    printf("test_null_safety\n");
    gpio_led_init(NULL, &mock_hal);
    gpio_led_on(NULL);
    gpio_led_off(NULL);
    gpio_led_blink(NULL, 100, 100, 0);
    gpio_led_breathe(NULL, 500);
    gpio_led_pulse(NULL, 200);
    gpio_led_tick(NULL, 10);
    ASSERT(gpio_led_get_mode(NULL) == GPIO_LED_OFF, "null returns OFF");
    /* no crash = pass */
    ASSERT(1, "null safety no crash");
}

static void test_zero_dt(void)
{
    gpio_led_t led;

    printf("test_zero_dt\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 100, 0);

    int calls_before = mock_idx;
    gpio_led_tick(&led, 0);
    ASSERT(mock_idx == calls_before, "zero dt is no-op");
}

static void test_custom_brightness(void)
{
    gpio_led_t led;

    printf("test_custom_brightness\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    led.brightness = 128;

    gpio_led_on(&led);
    ASSERT(mock_current == 128, "on at custom brightness");

    gpio_led_blink(&led, 100, 100, 0);
    ASSERT(mock_current == 128, "blink ON at custom brightness");

    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "blink OFF");

    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 128, "blink ON again at 128");
}

static void test_reinit_resets(void)
{
    gpio_led_t led;

    printf("test_reinit_resets\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_on(&led);
    ASSERT(mock_current == 255, "on before re-init");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_ON, "mode ON");

    gpio_led_init(&led, &mock_hal);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "mode OFF after re-init");
    ASSERT(mock_current == 0, "LED off after re-init");
}

static void test_blink_zero_on(void)
{
    gpio_led_t led;

    printf("test_blink_zero_on\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 0, 100, 0);

    gpio_led_tick(&led, 1);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK, "still blink mode");

    gpio_led_tick(&led, 1);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK, "still blink after 2 ticks");
}

static void test_pulse_zero_on(void)
{
    gpio_led_t led;

    printf("test_pulse_zero_on\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_pulse(&led, 0);

    gpio_led_tick(&led, 1);
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF, "pulse 0ms turns off immediately");
    ASSERT(mock_current == 0, "LED off after 0ms pulse");
}

static void test_breathe_period_one(void)
{
    gpio_led_t led;

    printf("test_breathe_period_one\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1);

    /* period=1, half=0→1: elapsed wraps to 0 every tick */
    gpio_led_tick(&led, 1);
    ASSERT(mock_current == 0, "period=1 wraps to 0");
    gpio_led_tick(&led, 1);
    ASSERT(mock_current == 0, "period=1 stays at 0");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BREATHE, "still breathing");
}

static void test_breathe_period_zero(void)
{
    gpio_led_t led;

    printf("test_breathe_period_zero\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 0);

    int calls_before = mock_idx;
    gpio_led_tick(&led, 100);
    ASSERT(mock_idx == calls_before, "period=0 tick is no-op");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BREATHE, "mode unchanged");
}

static void test_blink_zero_off(void)
{
    gpio_led_t led;

    printf("test_blink_zero_off\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_blink(&led, 100, 0, 0);

    ASSERT(mock_current == 255, "starts ON");

    gpio_led_tick(&led, 100);
    ASSERT(mock_current == 0, "OFF after 100ms");

    /* off_ms=0 clamped to 1: should return to ON after 1ms */
    gpio_led_tick(&led, 1);
    ASSERT(mock_current == 255, "back to ON after 1ms off");
}

static void test_breathe_large_dt(void)
{
    gpio_led_t led;

    printf("test_breathe_large_dt\n");
    mock_reset();
    gpio_led_init(&led, &mock_hal);
    gpio_led_breathe(&led, 1000);

    /* tick with dt larger than period — should wrap correctly */
    gpio_led_tick(&led, 1500);
    /* elapsed = 1500 - 1000 = 500. half=500. elapsed(500) < half(500)? No.
       level = (1000-500)*255/500 = 255. */
    ASSERT(mock_current > 240, "large dt wraps and reaches peak");
    ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BREATHE, "still breathing");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_init();
    test_on_off();
    test_blink_basic();
    test_blink_asymmetric();
    test_blink_repeat();
    test_blink_single();
    test_pulse();
    test_breathe_ramp();
    test_breathe_continuous();
    test_mode_switch();
    test_no_redundant_calls();
    test_null_safety();
    test_zero_dt();
    test_custom_brightness();
    test_reinit_resets();
    test_blink_zero_on();
    test_pulse_zero_on();
    test_breathe_period_one();
    test_breathe_period_zero();
    test_blink_zero_off();
    test_breathe_large_dt();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
