// SPDX-License-Identifier: MIT
/*
 * gpio_led — GPIO LED control (on/off, blink, breathe, pattern)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "gpio_led.h"
#include "contract.h"
#include <stddef.h>

static void emit(gpio_led_t *led, uint8_t level)
{
    if (led->last_level != level) {
        led->last_level = level;
        led->hal->set(led->hal->ctx, level);
    }
}

void gpio_led_init(gpio_led_t *led, const gpio_led_hal_t *hal)
{
    if (led == NULL || hal == NULL) {
        return;
    }
    led->hal        = hal;
    led->mode       = GPIO_LED_OFF;
    led->on_ms      = 0U;
    led->off_ms     = 0U;
    led->elapsed    = 0U;
    led->brightness = 255U;
    led->repeat     = 0U;
    led->remaining  = 0U;
    led->phase      = 0U;
    led->last_level = 0U;
    hal->set(hal->ctx, 0U);
}

void gpio_led_on(gpio_led_t *led)
{
    if (led == NULL) {
        return;
    }
    led->mode = GPIO_LED_ON;
    emit(led, led->brightness);
}

void gpio_led_off(gpio_led_t *led)
{
    if (led == NULL) {
        return;
    }
    led->mode = GPIO_LED_OFF;
    emit(led, 0U);
}

void gpio_led_blink(gpio_led_t *led, uint16_t on_ms,
                     uint16_t off_ms, uint8_t repeat)
{
    if (led == NULL) {
        return;
    }
    led->mode      = GPIO_LED_BLINK;
    led->on_ms     = on_ms;
    led->off_ms    = off_ms;
    led->elapsed   = 0U;
    led->phase     = 0U;
    led->repeat    = repeat;
    led->remaining = repeat;
    emit(led, led->brightness);
}

void gpio_led_breathe(gpio_led_t *led, uint16_t period_ms)
{
    if (led == NULL) {
        return;
    }
    led->mode    = GPIO_LED_BREATHE;
    led->on_ms   = period_ms;
    led->elapsed = 0U;
    emit(led, 0U);
}

void gpio_led_pulse(gpio_led_t *led, uint16_t on_ms)
{
    if (led == NULL) {
        return;
    }
    led->mode    = GPIO_LED_PULSE;
    led->on_ms   = on_ms;
    led->elapsed = 0U;
    emit(led, led->brightness);
}

gpio_led_mode_t gpio_led_get_mode(const gpio_led_t *led)
{
    if (led == NULL) {
        return GPIO_LED_OFF;
    }
    return led->mode;
}

static void tick_blink(gpio_led_t *led, uint16_t dt_ms)
{
    uint32_t threshold;

    led->elapsed += dt_ms;
    threshold = (led->phase == 0U) ? led->on_ms : led->off_ms;
    if (threshold == 0U) {
        threshold = 1U;
    }

    if (led->elapsed >= threshold) {
        led->elapsed -= threshold;
        if (led->phase == 0U) {
            led->phase = 1U;
            emit(led, 0U);
        } else {
            led->phase = 0U;
            if (led->repeat != 0U) {
                if (led->remaining > 1U) {
                    led->remaining--;
                } else {
                    led->mode = GPIO_LED_OFF;
                    emit(led, 0U);
                    return;
                }
            }
            emit(led, led->brightness);
        }
    }
}

static void tick_breathe(gpio_led_t *led, uint16_t dt_ms)
{
    uint32_t period = led->on_ms;
    uint32_t half;
    uint32_t level;

    if (period == 0U) {
        return;
    }

    led->elapsed += dt_ms;
    if (led->elapsed >= period) {
        led->elapsed -= period;
    }

    half = period / 2U;
    if (half == 0U) {
        half = 1U;
    }

    if (led->elapsed < half) {
        level = ((uint32_t)led->elapsed * led->brightness) / half;
    } else {
        level = ((uint32_t)(period - led->elapsed) * led->brightness) / half;
    }

    if (level > 255U) {
        level = 255U;
    }

    emit(led, (uint8_t)level);
}

static void tick_pulse(gpio_led_t *led, uint16_t dt_ms)
{
    led->elapsed += dt_ms;
    if (led->elapsed >= led->on_ms) {
        led->mode = GPIO_LED_OFF;
        emit(led, 0U);
    }
}

void gpio_led_tick(gpio_led_t *led, uint16_t dt_ms)
{
    if (led == NULL || dt_ms == 0U) {
        return;
    }

    switch (led->mode) {
    case GPIO_LED_BLINK:
        tick_blink(led, dt_ms);
        break;
    case GPIO_LED_BREATHE:
        tick_breathe(led, dt_ms);
        break;
    case GPIO_LED_PULSE:
        tick_pulse(led, dt_ms);
        break;
    case GPIO_LED_OFF:
    case GPIO_LED_ON:
    default:
        break;
    }
}
