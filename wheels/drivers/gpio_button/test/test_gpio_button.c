#include <stdio.h>
#include <string.h>
#include "../gpio_button.h"

/* ---------- mock HAL ---------- */

static uint8_t mock_level;

static void mock_set(uint8_t level)
{
    mock_level = level;
}

static uint8_t mock_read(void *ctx)
{
    (void)ctx;
    return mock_level;
}

static const gpio_btn_hal_t mock_hal = {
    .read = mock_read,
    .ctx  = NULL,
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

/* simulate ticking N ms in 1ms steps, return last non-NONE event */
static gpio_btn_evt_t tick_ms(gpio_btn_t *btn, uint16_t ms)
{
    gpio_btn_evt_t last = GPIO_BTN_EVT_NONE;
    for (uint16_t i = 0; i < ms; i++) {
        gpio_btn_evt_t e = gpio_btn_tick(btn, 1);
        if (e != GPIO_BTN_EVT_NONE) {
            last = e;
        }
    }
    return last;
}

/* ---------- tests ---------- */

static void test_default_cfg(void)
{
    printf("test_default_cfg\n");
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    ASSERT(cfg.debounce_ms == 20, "debounce 20ms");
    ASSERT(cfg.long_press_ms == 1000, "long press 1000ms");
    ASSERT(cfg.double_click_ms == 300, "double click 300ms");
    ASSERT(cfg.active_level == 0, "active low");
}

static void test_init_idle(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_init_idle\n");
    mock_level = 1;  /* inactive (active low) */
    gpio_btn_init(&btn, &mock_hal, &cfg);

    ASSERT(gpio_btn_is_pressed(&btn) == 0, "not pressed after init");

    gpio_btn_evt_t e = tick_ms(&btn, 100);
    ASSERT(e == GPIO_BTN_EVT_NONE, "no event when idle");
}

static void test_press_debounce(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_press_debounce\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press (active low → level 0) */
    mock_level = 0;
    gpio_btn_evt_t e = tick_ms(&btn, 10);
    ASSERT(e == GPIO_BTN_EVT_NONE, "no event before debounce");
    ASSERT(gpio_btn_is_pressed(&btn) == 0, "not pressed yet");

    e = tick_ms(&btn, 15);
    ASSERT(e == GPIO_BTN_EVT_PRESS, "press after debounce");
    ASSERT(gpio_btn_is_pressed(&btn) == 1, "now pressed");
}

static void test_bounce_rejection(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_bounce_rejection\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* bounce: press for 5ms, release, press again */
    mock_level = 0;
    tick_ms(&btn, 5);
    mock_level = 1;
    tick_ms(&btn, 2);
    mock_level = 0;
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    /* should get exactly one PRESS */
    ASSERT(e == GPIO_BTN_EVT_PRESS, "single press after bounce");
}

static void test_click(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_click\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press */
    mock_level = 0;
    tick_ms(&btn, 25);

    /* release after short press */
    mock_level = 1;
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT(e == GPIO_BTN_EVT_RELEASE, "release event");

    /* wait for double-click window to expire */
    e = tick_ms(&btn, 310);
    ASSERT(e == GPIO_BTN_EVT_CLICK, "click after dc window");
}

static void test_double_click(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_double_click\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* first press + release */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* second press + release within dc window */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT(e == GPIO_BTN_EVT_DOUBLE_CLICK, "double click");
}

static void test_long_press(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_long_press\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press and hold */
    mock_level = 0;
    tick_ms(&btn, 25);  /* debounce → PRESS */

    gpio_btn_evt_t e = tick_ms(&btn, 1000);
    ASSERT(e == GPIO_BTN_EVT_LONG_PRESS, "long press at 1000ms");
    ASSERT(gpio_btn_is_pressed(&btn) == 1, "still pressed");

    /* release after long press → no click */
    mock_level = 1;
    e = tick_ms(&btn, 25);
    ASSERT(e == GPIO_BTN_EVT_RELEASE, "release after long");

    /* no click event should follow */
    e = tick_ms(&btn, 400);
    ASSERT(e == GPIO_BTN_EVT_NONE, "no click after long press");
}

static void test_active_high(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    cfg.active_level = 1;

    printf("test_active_high\n");
    mock_level = 0;  /* inactive */
    gpio_btn_init(&btn, &mock_hal, &cfg);

    ASSERT(gpio_btn_is_pressed(&btn) == 0, "not pressed");

    mock_level = 1;  /* active */
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT(e == GPIO_BTN_EVT_PRESS, "press active high");
    ASSERT(gpio_btn_is_pressed(&btn) == 1, "pressed");
}

static void test_long_press_no_double_click(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_long_press_no_double_click\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* first click */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* second press but hold → long press, not double click */
    mock_level = 0;
    tick_ms(&btn, 25);
    gpio_btn_evt_t e = tick_ms(&btn, 1000);
    ASSERT(e == GPIO_BTN_EVT_LONG_PRESS, "long press not double click");
}

static void test_null_safety(void)
{
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_null_safety\n");
    gpio_btn_init(NULL, &mock_hal, &cfg);
    ASSERT(gpio_btn_tick(NULL, 10) == GPIO_BTN_EVT_NONE, "null tick");
    ASSERT(gpio_btn_is_pressed(NULL) == 0, "null is_pressed");
}

static void test_zero_dt(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_zero_dt\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);
    ASSERT(gpio_btn_tick(&btn, 0) == GPIO_BTN_EVT_NONE, "zero dt noop");
}

static void test_rapid_clicks_separated(void)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();

    printf("test_rapid_clicks_separated\n");
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* click, wait for dc window, click again → two separate clicks */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    gpio_btn_evt_t e = tick_ms(&btn, 310);
    ASSERT(e == GPIO_BTN_EVT_CLICK, "first click");

    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    e = tick_ms(&btn, 310);
    ASSERT(e == GPIO_BTN_EVT_CLICK, "second click");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_default_cfg();
    test_init_idle();
    test_press_debounce();
    test_bounce_rejection();
    test_click();
    test_double_click();
    test_long_press();
    test_active_high();
    test_long_press_no_double_click();
    test_null_safety();
    test_zero_dt();
    test_rapid_clicks_separated();

    printf("\n%d tests, %d failures\n", tests_run, tests_fail);
    return tests_fail ? 1 : 0;
}
