/*
 * gpio_button — GPIO button with debounce, long-press, multi-click
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    gpio_hal
 */

#include "gpio_button.h"
#include <stddef.h>

gpio_btn_cfg_t gpio_btn_default_cfg(void)
{
    gpio_btn_cfg_t cfg;
    cfg.debounce_ms     = 20U;
    cfg.long_press_ms   = 1000U;
    cfg.double_click_ms = 300U;
    cfg.active_level    = 0U;
    return cfg;
}

void gpio_btn_init(gpio_btn_t *btn, const gpio_btn_hal_t *hal,
                    const gpio_btn_cfg_t *cfg)
{
    if (btn == NULL || hal == NULL || cfg == NULL) {
        return;
    }
    btn->hal           = hal;
    btn->cfg           = *cfg;
    btn->raw           = !cfg->active_level;
    btn->stable        = btn->raw;
    btn->pressed       = 0U;
    btn->debounce_cnt  = 0U;
    btn->hold_cnt      = 0U;
    btn->idle_cnt      = 0U;
    btn->click_pending = 0U;
    btn->long_fired    = 0U;
}

uint8_t gpio_btn_is_pressed(const gpio_btn_t *btn)
{
    if (btn == NULL) {
        return 0U;
    }
    return btn->pressed;
}

gpio_btn_evt_t gpio_btn_tick(gpio_btn_t *btn, uint16_t dt_ms)
{
    uint8_t sample;
    uint8_t is_active;
    uint8_t state_changed = 0U;
    gpio_btn_evt_t evt = GPIO_BTN_EVT_NONE;

    if (btn == NULL || dt_ms == 0U) {
        return GPIO_BTN_EVT_NONE;
    }

    sample = btn->hal->read(btn->hal->ctx);

    /* debounce: restart counter on any raw change */
    if (sample != btn->raw) {
        btn->raw = sample;
        btn->debounce_cnt = 0U;
    } else {
        if (btn->debounce_cnt < btn->cfg.debounce_ms) {
            btn->debounce_cnt += dt_ms;
        }
    }

    /* only accept a new stable state after debounce completes */
    if (btn->debounce_cnt >= btn->cfg.debounce_ms && btn->raw != btn->stable) {
        btn->stable = btn->raw;
        state_changed = 1U;
    }

    is_active = (btn->stable == btn->cfg.active_level) ? 1U : 0U;

    if (state_changed) {
        if (is_active && !btn->pressed) {
            btn->pressed    = 1U;
            btn->hold_cnt   = 0U;
            btn->long_fired = 0U;
            btn->idle_cnt   = 0U;
            evt = GPIO_BTN_EVT_PRESS;
        } else if (!is_active && btn->pressed) {
            btn->pressed = 0U;
            evt = GPIO_BTN_EVT_RELEASE;

            if (!btn->long_fired) {
                if (btn->click_pending) {
                    btn->click_pending = 0U;
                    return GPIO_BTN_EVT_DOUBLE_CLICK;
                }
                btn->click_pending = 1U;
                btn->idle_cnt = 0U;
            }
        }
    } else if (btn->pressed) {
        btn->hold_cnt += dt_ms;
        if (!btn->long_fired && btn->hold_cnt >= btn->cfg.long_press_ms) {
            btn->long_fired = 1U;
            btn->click_pending = 0U;
            return GPIO_BTN_EVT_LONG_PRESS;
        }
    }

    /* pending click: emit after double-click window expires */
    if (btn->click_pending && !btn->pressed) {
        btn->idle_cnt += dt_ms;
        if (btn->idle_cnt >= btn->cfg.double_click_ms) {
            btn->click_pending = 0U;
            return GPIO_BTN_EVT_CLICK;
        }
    }

    return evt;
}
