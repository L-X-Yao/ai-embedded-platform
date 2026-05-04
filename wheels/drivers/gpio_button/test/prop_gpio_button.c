// SPDX-License-Identifier: MIT
/*
 * prop_gpio_button.c — property-based tests for GPIO button driver
 *
 * Properties:
 *   1. stable press (debounce_ms+1 ticks at active level) always detected
 *   2. bouncing within debounce window never triggers press
 *   3. tick with arbitrary dt values never crashes
 *   4. no event accumulation: after init, idle ticks produce no events
 */

#include "test_harness.h"
#include "property_test.h"
#include "gpio_button.h"

/* ---------- mock GPIO ---------- */

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

/* Tick N ms in 1ms steps, return last non-NONE event */
static gpio_button_evt_t tick_ms(gpio_button_t *btn, uint16_t ms)
{
    gpio_button_evt_t last = GPIO_BUTTON_EVT_NONE;
    for (uint16_t i = 0; i < ms; i++) {
        gpio_button_evt_t e = gpio_button_tick(btn, 1);
        if (e != GPIO_BUTTON_EVT_NONE) last = e;
    }
    return last;
}

/* ------------------------------------------------------------------ */

PROPERTY(stable_press_detected, 10000)
{
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    /* Random debounce 5-50ms */
    cfg.debounce_ms = (uint16_t)(5 + ARBI_UINT8(seed) % 46);
    cfg.active_level = ARBI_UINT8(seed) & 1;
    uint8_t inactive = cfg.active_level ? 0 : 1;
    uint8_t active   = cfg.active_level;

    gpio_button_t btn;
    mock_level = inactive;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* Idle for a bit */
    tick_ms(&btn, 10);
    PROP_ASSERT(gpio_button_is_pressed(&btn) == 0);

    /* Press and hold for debounce_ms + some margin.
     * The debounce counter resets on first change tick, then counts
     * subsequent ticks. Need debounce_ms + 1 sustained ticks minimum. */
    mock_level = active;
    gpio_button_evt_t e = tick_ms(&btn, (uint16_t)(cfg.debounce_ms + 5));

    /* After debounce_ms+5 ticks of stable active level, PRESS must fire */
    PROP_ASSERT(e == GPIO_BUTTON_EVT_PRESS);
    PROP_ASSERT(gpio_button_is_pressed(&btn) == 1);
}

PROPERTY(bounce_filtered, 5000)
{
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    cfg.debounce_ms = (uint16_t)(10 + ARBI_UINT8(seed) % 40);
    cfg.active_level = 0; /* active low */

    gpio_button_t btn;
    mock_level = 1; /* inactive */
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* Simulate bouncing: press for less than debounce, release, repeat */
    uint16_t bounce_time = (uint16_t)(1 + ARBI_UINT8(seed) % (cfg.debounce_ms - 1));
    if (bounce_time >= cfg.debounce_ms) bounce_time = (uint16_t)(cfg.debounce_ms - 1);

    mock_level = 0; /* active */
    gpio_button_evt_t e = tick_ms(&btn, bounce_time);
    mock_level = 1; /* release before debounce */
    gpio_button_evt_t e2 = tick_ms(&btn, 5);

    /* Neither segment should produce a PRESS event */
    PROP_ASSERT(e != GPIO_BUTTON_EVT_PRESS);
    PROP_ASSERT(e2 != GPIO_BUTTON_EVT_PRESS);
    PROP_ASSERT(gpio_button_is_pressed(&btn) == 0);
}

PROPERTY(arbitrary_dt_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    gpio_button_t btn;
    mock_level = 1;
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* Feed random dt values and random level changes */
    for (int i = 0; i < 5; i++) {
        mock_level = ARBI_UINT8(seed) & 1;
        uint16_t dt = ARBI_UINT16(seed);
        gpio_button_tick(&btn, dt);
    }
    /* Must not crash */
}

PROPERTY(idle_no_events, 5000)
{
    gpio_button_cfg_t cfg = gpio_button_default_cfg();
    gpio_button_t btn;
    mock_level = 1; /* inactive (active_level=0 default) */
    gpio_button_init(&btn, &mock_hal, &cfg);

    /* Tick for a random duration while idle — no events should fire */
    uint16_t duration = (uint16_t)(100 + ARBI_UINT16(seed) % 1000);
    gpio_button_evt_t e = tick_ms(&btn, duration);
    PROP_ASSERT(e == GPIO_BUTTON_EVT_NONE);
    PROP_ASSERT(gpio_button_is_pressed(&btn) == 0);
}

TEST_MAIN("prop_gpio_button")
    RUN(stable_press_detected);
    RUN(bounce_filtered);
    RUN(arbitrary_dt_no_crash);
    RUN(idle_no_events);
TEST_MAIN_END()
