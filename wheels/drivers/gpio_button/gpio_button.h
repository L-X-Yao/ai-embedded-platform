#ifndef GPIO_BUTTON_H
#define GPIO_BUTTON_H

#include <stdint.h>

/* ---------- events ---------- */

typedef enum {
    GPIO_BTN_EVT_NONE = 0,
    GPIO_BTN_EVT_PRESS,
    GPIO_BTN_EVT_RELEASE,
    GPIO_BTN_EVT_CLICK,
    GPIO_BTN_EVT_DOUBLE_CLICK,
    GPIO_BTN_EVT_LONG_PRESS,
} gpio_btn_evt_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    uint8_t (*read)(void *ctx);
    void *ctx;
} gpio_btn_hal_t;

/* ---------- configuration ---------- */

typedef struct {
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t double_click_ms;
    uint8_t  active_level;
} gpio_btn_cfg_t;

/* ---------- instance state ---------- */

typedef struct {
    const gpio_btn_hal_t *hal;
    gpio_btn_cfg_t cfg;
    uint8_t  raw;
    uint8_t  stable;
    uint8_t  pressed;
    uint16_t debounce_cnt;
    uint16_t hold_cnt;
    uint16_t idle_cnt;
    uint8_t  click_pending;
    uint8_t  long_fired;
} gpio_btn_t;

/* ---------- API ---------- */

gpio_btn_cfg_t gpio_btn_default_cfg(void);
void           gpio_btn_init(gpio_btn_t *btn, const gpio_btn_hal_t *hal,
                              const gpio_btn_cfg_t *cfg);
gpio_btn_evt_t gpio_btn_tick(gpio_btn_t *btn, uint16_t dt_ms);
uint8_t        gpio_btn_is_pressed(const gpio_btn_t *btn);

#endif /* GPIO_BUTTON_H */
