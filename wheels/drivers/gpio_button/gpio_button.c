// SPDX-License-Identifier: MIT
/*
 * gpio_button — GPIO button with debounce, long-press, multi-click
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "gpio_button.h"
#include "contract.h"
#include <stddef.h>
#include <string.h>

gpio_button_cfg_t gpio_button_default_cfg(void)
{
    gpio_button_cfg_t cfg;
    cfg.debounce_ms     = 20U;
    cfg.long_press_ms   = 1000U;
    cfg.double_click_ms = 300U;
    cfg.active_level    = 0U;
    return cfg;
}

void gpio_button_init(gpio_button_t *btn, const gpio_button_hal_t *hal,
                    const gpio_button_cfg_t *cfg)
{
    if (btn == NULL) {
        return;
    }

    /* btn->hal stays NULL unless init accepts its arguments: init cannot say
     * no, so the tick refuses a button without it */
    memset(btn, 0, sizeof(*btn));

    if (hal == NULL || cfg == NULL) {
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
    btn->next_evt      = (uint8_t)GPIO_BUTTON_EVT_NONE;
}

uint8_t gpio_button_is_pressed(const gpio_button_t *btn)
{
    if (btn == NULL) {
        return 0U;
    }
    return btn->pressed;
}

gpio_button_evt_t gpio_button_tick(gpio_button_t *btn, uint16_t dt_ms)
{
    uint8_t sample;
    uint8_t is_active;
    uint8_t state_changed = 0U;
    gpio_button_evt_t evt = GPIO_BUTTON_EVT_NONE;

    if (btn == NULL || btn->hal == NULL || btn->hal->read == NULL ||
        dt_ms == 0U) {
        return GPIO_BUTTON_EVT_NONE;
    }

    /* RELEASE went out on the last tick and its gesture goes now. Not reading
     * the pin on this tick means it can never make a second event, so one
     * slot is enough however fast the edges come with debounce_ms 0. */
    if (btn->next_evt != (uint8_t)GPIO_BUTTON_EVT_NONE) {
        evt = (gpio_button_evt_t)btn->next_evt;
        btn->next_evt = (uint8_t)GPIO_BUTTON_EVT_NONE;
        return evt;
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
            evt = GPIO_BUTTON_EVT_PRESS;
        } else if (!is_active && btn->pressed) {
            btn->pressed = 0U;
            evt = GPIO_BUTTON_EVT_RELEASE;

            if (!btn->long_fired) {
                if (btn->click_pending) {
                    btn->click_pending = 0U;
                    btn->next_evt = (uint8_t)GPIO_BUTTON_EVT_DOUBLE_CLICK;
                    return evt;
                }
                btn->click_pending = 1U;
                btn->idle_cnt = 0U;
            }
        }
    } else if (btn->pressed) {
        if (btn->hold_cnt <= UINT32_MAX - dt_ms)
            btn->hold_cnt += dt_ms;
        if (!btn->long_fired && btn->hold_cnt >= btn->cfg.long_press_ms) {
            btn->long_fired = 1U;
            btn->click_pending = 0U;
            return GPIO_BUTTON_EVT_LONG_PRESS;
        }
    }

    /* pending click: emit after double-click window expires */
    if (btn->click_pending && !btn->pressed) {
        if (btn->idle_cnt <= UINT32_MAX - dt_ms)
            btn->idle_cnt += dt_ms;
        if (btn->idle_cnt >= btn->cfg.double_click_ms) {
            btn->click_pending = 0U;
            /* A window under one tick ends on the release tick itself */
            if (evt != GPIO_BUTTON_EVT_NONE) {
                btn->next_evt = (uint8_t)GPIO_BUTTON_EVT_CLICK;
                return evt;
            }
            return GPIO_BUTTON_EVT_CLICK;
        }
    }

    return evt;
}
