// SPDX-License-Identifier: MIT
/*
 * gpio_button — GPIO button with debounce, long-press, multi-click
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#ifndef GPIO_BUTTON_H
#define GPIO_BUTTON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- events ---------- */

typedef enum {
    GPIO_BUTTON_EVT_NONE = 0,
    GPIO_BUTTON_EVT_PRESS,
    GPIO_BUTTON_EVT_RELEASE,
    GPIO_BUTTON_EVT_CLICK,
    GPIO_BUTTON_EVT_DOUBLE_CLICK,
    GPIO_BUTTON_EVT_LONG_PRESS,
} gpio_button_evt_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    uint8_t (*read)(void *ctx);
    void *ctx;
} gpio_button_hal_t;

/* ---------- configuration ---------- */

typedef struct {
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t double_click_ms;
    uint8_t  active_level;
} gpio_button_cfg_t;

/* ---------- instance state ---------- */

typedef struct {
    const gpio_button_hal_t *hal;
    gpio_button_cfg_t cfg;
    uint8_t  raw;
    uint8_t  stable;
    uint8_t  pressed;
    uint32_t debounce_cnt;
    uint32_t hold_cnt;
    uint32_t idle_cnt;
    uint8_t  click_pending;
    uint8_t  long_fired;
} gpio_button_t;

/* ---------- API ---------- */

/** Return sensible default configuration. @thread_safe: yes (pure) */
gpio_button_cfg_t gpio_button_default_cfg(void);
/** Initialize button instance. @thread_safe: no */
void           gpio_button_init(gpio_button_t *btn, const gpio_button_hal_t *hal,
                              const gpio_button_cfg_t *cfg);
/** Advance debounce/click state machine. @thread_safe: caller_must_lock */
gpio_button_evt_t gpio_button_tick(gpio_button_t *btn, uint16_t dt_ms);
/** Query current stable press state. @thread_safe: caller_must_lock */
uint8_t        gpio_button_is_pressed(const gpio_button_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_BUTTON_H */
