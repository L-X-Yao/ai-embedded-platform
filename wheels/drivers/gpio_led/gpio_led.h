/*
 * gpio_led — GPIO LED control (on/off, blink, breathe, pattern)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    gpio_hal
 */

#ifndef GPIO_LED_H
#define GPIO_LED_H

#include <stdint.h>

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*set)(void *ctx, uint8_t level);
    void *ctx;
} gpio_led_hal_t;

/* ---------- modes ---------- */

typedef enum {
    GPIO_LED_OFF = 0,
    GPIO_LED_ON,
    GPIO_LED_BLINK,
    GPIO_LED_BREATHE,
    GPIO_LED_PULSE,
} gpio_led_mode_t;

/* ---------- instance state ---------- */

typedef struct {
    const gpio_led_hal_t *hal;
    gpio_led_mode_t mode;
    uint16_t on_ms;
    uint16_t off_ms;
    uint16_t elapsed;
    uint8_t  brightness;
    uint8_t  repeat;
    uint8_t  remaining;
    uint8_t  phase;
    uint8_t  last_level;
} gpio_led_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize LED instance. @thread_safe: no */
void            gpio_led_init(gpio_led_t *led, const gpio_led_hal_t *hal);
/** Turn LED on (steady). @thread_safe: caller_must_lock */
void            gpio_led_on(gpio_led_t *led);
/** Turn LED off. @thread_safe: caller_must_lock */
void            gpio_led_off(gpio_led_t *led);
/** Start blink pattern. @thread_safe: caller_must_lock */
void            gpio_led_blink(gpio_led_t *led, uint16_t on_ms,
                               uint16_t off_ms, uint8_t repeat);
/** Start breathing effect. @thread_safe: caller_must_lock */
void            gpio_led_breathe(gpio_led_t *led, uint16_t period_ms);
/** Single pulse then off. @thread_safe: caller_must_lock */
void            gpio_led_pulse(gpio_led_t *led, uint16_t on_ms);
/** Advance state machine by dt_ms. @thread_safe: caller_must_lock */
void            gpio_led_tick(gpio_led_t *led, uint16_t dt_ms);
/** Query current mode. @thread_safe: caller_must_lock */
gpio_led_mode_t gpio_led_get_mode(const gpio_led_t *led);

#endif /* GPIO_LED_H */
