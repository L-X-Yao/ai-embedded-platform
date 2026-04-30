#include "test_harness.h"
#include <string.h>
#include "gpio_button.h"

/* ---------- mock HAL ---------- */

static uint8_t mock_level;

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

TEST(test_default_cfg)
{
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    ASSERT_TRUE(cfg.debounce_ms == 20);
    ASSERT_TRUE(cfg.long_press_ms == 1000);
    ASSERT_TRUE(cfg.double_click_ms == 300);
    ASSERT_TRUE(cfg.active_level == 0);
}

TEST(test_init_idle)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;  /* inactive (active low) */
    gpio_btn_init(&btn, &mock_hal, &cfg);

    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 0);

    gpio_btn_evt_t e = tick_ms(&btn, 100);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);
}

TEST(test_press_debounce)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press (active low → level 0) */
    mock_level = 0;
    gpio_btn_evt_t e = tick_ms(&btn, 10);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 0);

    e = tick_ms(&btn, 15);
    ASSERT_TRUE(e == GPIO_BTN_EVT_PRESS);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);
}

TEST(test_bounce_rejection)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
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
    ASSERT_TRUE(e == GPIO_BTN_EVT_PRESS);
}

TEST(test_click)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press */
    mock_level = 0;
    tick_ms(&btn, 25);

    /* release after short press */
    mock_level = 1;
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BTN_EVT_RELEASE);

    /* wait for double-click window to expire */
    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BTN_EVT_CLICK);
}

TEST(test_double_click)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
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
    ASSERT_TRUE(e == GPIO_BTN_EVT_DOUBLE_CLICK);
}

TEST(test_long_press)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* press and hold */
    mock_level = 0;
    tick_ms(&btn, 25);  /* debounce → PRESS */

    gpio_btn_evt_t e = tick_ms(&btn, 1000);
    ASSERT_TRUE(e == GPIO_BTN_EVT_LONG_PRESS);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);

    /* release after long press → no click */
    mock_level = 1;
    e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BTN_EVT_RELEASE);

    /* no click event should follow */
    e = tick_ms(&btn, 400);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);
}

TEST(test_active_high)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    cfg.active_level = 1;
    mock_level = 0;  /* inactive */
    gpio_btn_init(&btn, &mock_hal, &cfg);

    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 0);

    mock_level = 1;  /* active */
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BTN_EVT_PRESS);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);
}

TEST(test_long_press_no_double_click)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
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
    ASSERT_TRUE(e == GPIO_BTN_EVT_LONG_PRESS);
}

TEST(test_null_safety)
{
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    gpio_btn_init(NULL, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_btn_tick(NULL, 10) == GPIO_BTN_EVT_NONE);
    ASSERT_TRUE(gpio_btn_is_pressed(NULL) == 0);
}

TEST(test_zero_dt)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_btn_tick(&btn, 0) == GPIO_BTN_EVT_NONE);
}

TEST(test_rapid_clicks_separated)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* click, wait for dc window, click again → two separate clicks */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    gpio_btn_evt_t e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BTN_EVT_CLICK);

    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BTN_EVT_CLICK);
}

TEST(test_reinit_resets_state)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    tick_ms(&btn, 25);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);

    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 0);
    ASSERT_TRUE(gpio_btn_tick(&btn, 1) == GPIO_BTN_EVT_NONE);
}

TEST(test_debounce_boundary)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);
    mock_level = 0;
    gpio_btn_evt_t e = tick_ms(&btn, 19);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 0);

    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);
    mock_level = 0;
    e = tick_ms(&btn, 21);
    ASSERT_TRUE(e == GPIO_BTN_EVT_PRESS);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);
}

TEST(test_barely_miss_long_press)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    tick_ms(&btn, 25);  /* PRESS at tick 21, hold_cnt=4 after 25ms */

    gpio_btn_evt_t e = tick_ms(&btn, 975);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);

    mock_level = 1;
    e = tick_ms(&btn, 25);
    /* hold_cnt = 4 + 975 + 20 = 999 at state change — below 1000 */
    ASSERT_TRUE(e == GPIO_BTN_EVT_RELEASE);

    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BTN_EVT_CLICK);
}

TEST(test_triple_click)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* 1st press+release */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* 2nd press+release → DOUBLE_CLICK */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    gpio_btn_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BTN_EVT_DOUBLE_CLICK);

    /* 3rd press+release → new click_pending */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BTN_EVT_RELEASE);

    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BTN_EVT_CLICK);
}

TEST(test_large_dt)
{
    gpio_btn_t btn;
    gpio_btn_cfg_t cfg = gpio_btn_default_cfg();
    mock_level = 1;
    gpio_btn_init(&btn, &mock_hal, &cfg);

    /* single large tick while idle — no crash */
    gpio_btn_evt_t e = gpio_btn_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);

    /* level change tick resets debounce — no press on change tick */
    mock_level = 0;
    e = gpio_btn_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BTN_EVT_NONE);

    /* sustained level with large dt → debounce completes instantly */
    e = gpio_btn_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BTN_EVT_PRESS);
    ASSERT_TRUE(gpio_btn_is_pressed(&btn) == 1);
}

TEST_MAIN("gpio_button")
    RUN(test_default_cfg);
    RUN(test_init_idle);
    RUN(test_press_debounce);
    RUN(test_bounce_rejection);
    RUN(test_click);
    RUN(test_double_click);
    RUN(test_long_press);
    RUN(test_active_high);
    RUN(test_long_press_no_double_click);
    RUN(test_null_safety);
    RUN(test_zero_dt);
    RUN(test_rapid_clicks_separated);
    RUN(test_reinit_resets_state);
    RUN(test_debounce_boundary);
    RUN(test_barely_miss_long_press);
    RUN(test_triple_click);
    RUN(test_large_dt);
TEST_MAIN_END()

