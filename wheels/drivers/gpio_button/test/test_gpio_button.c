// SPDX-License-Identifier: MIT
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

static const gpio_button_hal_t mock_hal = {
    .read = mock_read,
    .ctx  = NULL,
};

/* ---------- test helpers ---------- */
/* simulate ticking N ms in 1ms steps, return last non-NONE event */
static gpio_button_evt_t tick_ms(gpio_button_t *btn, uint16_t ms)
{
    gpio_button_evt_t last = GPIO_BUTTON_EVT_NONE;
    for (uint16_t i = 0; i < ms; i++) {
        gpio_button_evt_t e = gpio_button_tick(btn, 1);
        if (e != GPIO_BUTTON_EVT_NONE) {
            last = e;
        }
    }
    return last;
}

/* ---------- tests ---------- */

TEST(test_default_cfg)
{
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    ASSERT_TRUE(cfg.debounce_ms == 20);
    ASSERT_TRUE(cfg.long_press_ms == 1000);
    ASSERT_TRUE(cfg.double_click_ms == 300);
    ASSERT_TRUE(cfg.active_level == 0);
}

TEST(test_init_idle)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;  /* inactive (active low) */
    gpio_button_init(&btn, &mock_hal, &cfg);

    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 0);

    gpio_button_evt_t e = tick_ms(&btn, 100);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);
}

TEST(test_press_debounce)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* press (active low → level 0) */
    mock_level = 0;
    gpio_button_evt_t e = tick_ms(&btn, 10);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 0);

    e = tick_ms(&btn, 15);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_PRESS);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);
}

TEST(test_bounce_rejection)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* bounce: press for 5ms, release, press again */
    mock_level = 0;
    tick_ms(&btn, 5);
    mock_level = 1;
    tick_ms(&btn, 2);
    mock_level = 0;
    gpio_button_evt_t e = tick_ms(&btn, 25);
    /* should get exactly one PRESS */
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_PRESS);
}

TEST(test_click)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* press */
    mock_level = 0;
    tick_ms(&btn, 25);

    /* release after short press */
    mock_level = 1;
    gpio_button_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_RELEASE);

    /* wait for double-click window to expire */
    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_CLICK);
}

TEST(test_double_click)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* first press + release */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* second press + release within dc window */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    gpio_button_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_DOUBLE_CLICK);
}

TEST(test_long_press)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* press and hold */
    mock_level = 0;
    tick_ms(&btn, 25);  /* debounce → PRESS */

    gpio_button_evt_t e = tick_ms(&btn, 1000);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_LONG_PRESS);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);

    /* release after long press → no click */
    mock_level = 1;
    e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_RELEASE);

    /* no click event should follow */
    e = tick_ms(&btn, 400);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);
}

TEST(test_active_high)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    cfg.active_level = 1;
    mock_level = 0;  /* inactive */
    gpio_button_init(&btn, &mock_hal, &cfg);

    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 0);

    mock_level = 1;  /* active */
    gpio_button_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_PRESS);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);
}

TEST(test_long_press_no_double_click)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* first click */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* second press but hold → long press, not double click */
    mock_level = 0;
    tick_ms(&btn, 25);
    gpio_button_evt_t e = tick_ms(&btn, 1000);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_LONG_PRESS);
}

TEST(test_null_safety)
{
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    gpio_button_init(NULL, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_button_tick(NULL, 10) == GPIO_BUTTON_EVT_NONE);
    ASSERT_TRUE(gpio_button_is_pressed(NULL) == 0);
}

TEST(test_zero_dt)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_button_tick(&btn, 0) == GPIO_BUTTON_EVT_NONE);
}

TEST(test_rapid_clicks_separated)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* click, wait for dc window, click again → two separate clicks */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    gpio_button_evt_t e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_CLICK);

    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);
    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_CLICK);
}

TEST(test_reinit_resets_state)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    tick_ms(&btn, 25);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);

    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 0);
    ASSERT_TRUE(gpio_button_tick(&btn, 1) == GPIO_BUTTON_EVT_NONE);
}

TEST(test_debounce_boundary)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);
    mock_level = 0;
    gpio_button_evt_t e = tick_ms(&btn, 19);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 0);

    /* debounce_ms=20 with 1ms ticks: first tick resets counter (raw change),
     * then 19 more ticks accumulate to 19 < 20, so 20ms is NOT enough.
     * 21st tick reaches counter=20 >= threshold → state_changed. */
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);
    mock_level = 0;
    e = tick_ms(&btn, 20);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);

    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);
    mock_level = 0;
    e = tick_ms(&btn, 21);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_PRESS);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);
}

TEST(test_barely_miss_long_press)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    tick_ms(&btn, 25);  /* PRESS at tick 21, hold_cnt=4 after 25ms */

    gpio_button_evt_t e = tick_ms(&btn, 975);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);

    mock_level = 1;
    e = tick_ms(&btn, 25);
    /* hold_cnt = 4 + 975 + 20 = 999 at state change — below 1000 */
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_RELEASE);

    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_CLICK);
}

TEST(test_triple_click)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* 1st press+release */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    tick_ms(&btn, 25);

    /* 2nd press+release → DOUBLE_CLICK */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    gpio_button_evt_t e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_DOUBLE_CLICK);

    /* 3rd press+release → new click_pending */
    mock_level = 0;
    tick_ms(&btn, 25);
    mock_level = 1;
    e = tick_ms(&btn, 25);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_RELEASE);

    e = tick_ms(&btn, 310);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_CLICK);
}

TEST(test_large_dt)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* single large tick while idle — no crash */
    gpio_button_evt_t e = gpio_button_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);

    /* level change tick resets debounce — no press on change tick */
    mock_level = 0;
    e = gpio_button_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_NONE);

    /* sustained level with large dt → debounce completes instantly */
    e = gpio_button_tick(&btn, 10000);
    ASSERT_TRUE(e == GPIO_BUTTON_EVT_PRESS);
    ASSERT_TRUE(gpio_button_is_pressed(&btn) == 1);
}

/* ---------- a button init did not accept ---------- */

static void assert_button_refused(gpio_button_t *b)
{
    ASSERT_EQ(gpio_button_tick(b, 10), GPIO_BUTTON_EVT_NONE);
    ASSERT_EQ(gpio_button_is_pressed(b), 0);
}

TEST(test_zeroed_button_refused)
{
    static gpio_button_t zeroed;
    assert_button_refused(&zeroed);
}

/* btn holds whatever the stack did, as nothing else wrote it */
TEST(test_button_refused_after_rejected_init)
{
    gpio_button_t b;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();

    memset(&b, 0xA5, sizeof(b));
    gpio_button_init(&b, NULL, &cfg);
    assert_button_refused(&b);

    memset(&b, 0xA5, sizeof(b));
    gpio_button_init(&b, &mock_hal, NULL);
    assert_button_refused(&b);
}

/* init returns nothing, so the tick has to catch a HAL with no read */
TEST(test_tick_refuses_a_hal_without_read)
{
    gpio_button_t b;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    gpio_button_hal_t no_read = { .read = NULL, .ctx = NULL };

    gpio_button_init(&b, &no_read, &cfg);
    assert_button_refused(&b);
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
    RUN(test_zeroed_button_refused);
    RUN(test_button_refused_after_rejected_init);
    RUN(test_tick_refuses_a_hal_without_read);
TEST_MAIN_END()

