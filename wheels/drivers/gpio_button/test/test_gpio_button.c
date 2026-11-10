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

/* ---------- every event, not just the last ---------- */

#define EVT_LOG_MAX 16
static gpio_button_evt_t evt_log[EVT_LOG_MAX];
static uint16_t          evt_log_len;

static void log_ticks(gpio_button_t *btn, uint16_t ticks, uint16_t dt_ms)
{
    for (uint16_t i = 0; i < ticks; i++) {
        gpio_button_evt_t e = gpio_button_tick(btn, dt_ms);
        if (e != GPIO_BUTTON_EVT_NONE && evt_log_len < EVT_LOG_MAX) {
            evt_log[evt_log_len++] = e;
        }
    }
}

TEST(test_double_click_keeps_every_release)
{
    static const gpio_button_evt_t want[] = {
        GPIO_BUTTON_EVT_PRESS, GPIO_BUTTON_EVT_RELEASE,
        GPIO_BUTTON_EVT_PRESS, GPIO_BUTTON_EVT_RELEASE,
        GPIO_BUTTON_EVT_DOUBLE_CLICK,
    };
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    evt_log_len = 0;
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    for (int click = 0; click < 2; click++) {
        mock_level = 0;
        log_ticks(&btn, 25, 1);
        mock_level = 1;
        log_ticks(&btn, 25, 1);
    }

    ASSERT_EQ(evt_log_len, 5);
    for (uint16_t i = 0; i < 5; i++) {
        ASSERT_EQ(evt_log[i], want[i]);
    }
}

TEST(test_click_window_under_a_tick_keeps_the_release)
{
    /* double_click_ms 5 with 10 ms ticks: the window ends on the release
     * tick itself, so CLICK has to wait for the next one. */
    static const gpio_button_evt_t want[] = {
        GPIO_BUTTON_EVT_PRESS, GPIO_BUTTON_EVT_RELEASE, GPIO_BUTTON_EVT_CLICK,
    };
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    cfg.double_click_ms = 5;
    evt_log_len = 0;
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    log_ticks(&btn, 5, 10);
    mock_level = 1;
    log_ticks(&btn, 5, 10);

    ASSERT_EQ(evt_log_len, 3);
    for (uint16_t i = 0; i < 3; i++) {
        ASSERT_EQ(evt_log[i], want[i]);
    }
}

TEST(test_double_click_comes_one_tick_after_the_release)
{
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    cfg.debounce_ms = 2;
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    mock_level = 0;
    tick_ms(&btn, 5);
    mock_level = 1;
    tick_ms(&btn, 5);
    mock_level = 0;
    tick_ms(&btn, 5);
    mock_level = 1;

    /* The change is seen on tick 1, and 2 ms of debounce end on tick 3. */
    ASSERT_EQ(gpio_button_tick(&btn, 1), GPIO_BUTTON_EVT_NONE);
    ASSERT_EQ(gpio_button_tick(&btn, 1), GPIO_BUTTON_EVT_NONE);
    ASSERT_EQ(gpio_button_tick(&btn, 1), GPIO_BUTTON_EVT_RELEASE);
    ASSERT_EQ(gpio_button_tick(&btn, 1), GPIO_BUTTON_EVT_DOUBLE_CLICK);
    ASSERT_EQ(gpio_button_tick(&btn, 1), GPIO_BUTTON_EVT_NONE);
}

TEST(test_gesture_tick_does_not_lose_the_next_press)
{
    /* No debounce, so edges can land on consecutive ticks: a press right
     * behind the second release must survive the tick the gesture uses. */
    static const gpio_button_evt_t want[] = {
        GPIO_BUTTON_EVT_PRESS, GPIO_BUTTON_EVT_RELEASE,
        GPIO_BUTTON_EVT_PRESS, GPIO_BUTTON_EVT_RELEASE,
        GPIO_BUTTON_EVT_DOUBLE_CLICK, GPIO_BUTTON_EVT_PRESS,
        GPIO_BUTTON_EVT_RELEASE,
    };
    static const uint8_t level[] = { 0, 1, 0, 1, 0, 0, 1 };
    gpio_button_t btn;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    cfg.debounce_ms = 0;
    evt_log_len = 0;
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    for (uint16_t i = 0; i < 7; i++) {
        mock_level = level[i];
        log_ticks(&btn, 1, 1);
    }

    ASSERT_EQ(evt_log_len, 7);
    for (uint16_t i = 0; i < 7; i++) {
        ASSERT_EQ(evt_log[i], want[i]);
    }
}

TEST(test_press_and_release_always_alternate)
{
    /* debounce, long press, double click, active level */
    static const gpio_button_cfg_t cfgs[] = {
        {  20, 1000, 300, 0 },
        {   0, 1000, 300, 0 },
        {   0,   30,   5, 1 },
        {   5,   60,   1, 0 },
        {  10,   20,  10, 1 },
    };

    uint32_t lcg = 1u;
    for (size_t c = 0; c < sizeof(cfgs) / sizeof(cfgs[0]); c++) {
        gpio_button_t btn;
        uint8_t held = 0;
        uint8_t idle = (uint8_t)!cfgs[c].active_level;

        mock_level = idle;
        gpio_button_init(&btn, &mock_hal, &cfgs[c]);

        for (uint32_t t = 0; t < 40000u; t++) {
            lcg = lcg * 1664525u + 1013904223u;
            if (t >= 39000u) {
                mock_level = idle;              /* end with the button up */
            } else if (((lcg >> 16) & 7u) == 0u) {
                mock_level = (uint8_t)!mock_level;
            }

            gpio_button_evt_t e = gpio_button_tick(&btn, 10);
            if (e == GPIO_BUTTON_EVT_PRESS) {
                ASSERT_EQ(held, 0);
                held = 1;
            } else if (e == GPIO_BUTTON_EVT_RELEASE) {
                ASSERT_EQ(held, 1);
                held = 0;
            }
            ASSERT_EQ(gpio_button_is_pressed(&btn), held);
        }
        ASSERT_EQ(held, 0);
    }
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
    RUN(test_double_click_keeps_every_release);
    RUN(test_click_window_under_a_tick_keeps_the_release);
    RUN(test_double_click_comes_one_tick_after_the_release);
    RUN(test_gesture_tick_does_not_lose_the_next_press);
    RUN(test_press_and_release_always_alternate);
TEST_MAIN_END()

