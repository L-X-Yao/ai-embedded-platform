// SPDX-License-Identifier: MIT
/*
 * mock_gpio.h — shared GPIO mock for L1 driver tests
 *
 * Header-only. Provides pin state tracking for GPIO-based drivers
 * (buttons, LEDs, stepper, keypad, rotary encoder, etc.)
 *
 * Function signatures match actual wheel HAL patterns:
 *   - gpio_led_hal_t:    void (*set)(void *ctx, uint8_t level)
 *   - gpio_button_hal_t: uint8_t (*read)(void *ctx)
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "mock_gpio.h"
 *   #include "my_driver.h"
 *
 *   static void mock_reset(void) {
 *       mock_gpio_reset();
 *       mock_gpio_pins[0] = 1;  // pin 0 starts high
 *   }
 *
 *   // For output drivers (LED, stepper):
 *   static const gpio_led_hal_t led_hal = {
 *       .set = mock_gpio_set,
 *       .ctx = mock_gpio_make_ctx(0),
 *   };
 *
 *   // For input drivers (button, encoder):
 *   static const gpio_button_hal_t btn_hal = {
 *       .read = mock_gpio_read,
 *       .ctx  = mock_gpio_make_ctx(1),
 *   };
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include <string.h>

#ifndef MOCK_GPIO_PIN_COUNT
#define MOCK_GPIO_PIN_COUNT 16
#endif

static uint8_t  mock_gpio_pins[MOCK_GPIO_PIN_COUNT];
static uint8_t  mock_gpio_dir[MOCK_GPIO_PIN_COUNT];
static int      mock_gpio_write_count;
static int      mock_gpio_read_count;
static int      mock_gpio_fail_next;
static int      mock_gpio_fail_at;
static int      mock_gpio_op_count;
static uint32_t mock_gpio_delay_count;

__attribute__((unused))
static void *mock_gpio_make_ctx(uint8_t pin)
{
    return (void *)(uintptr_t)pin;
}

static uint8_t mock_gpio_pin_from_ctx(void *ctx)
{
    if (ctx == NULL) return 0;
    uint8_t pin = (uint8_t)(uintptr_t)ctx;
    return (uint8_t)(pin % MOCK_GPIO_PIN_COUNT);
}

static void mock_gpio_reset(void)
{
    memset(mock_gpio_pins, 0, sizeof(mock_gpio_pins));
    memset(mock_gpio_dir,  0, sizeof(mock_gpio_dir));
    mock_gpio_write_count = 0;
    mock_gpio_read_count  = 0;
    mock_gpio_fail_next   = 0;
    mock_gpio_fail_at     = -1;
    mock_gpio_op_count    = 0;
    mock_gpio_delay_count = 0;
}

static int mock_gpio_should_fail(void)
{
    int n = mock_gpio_op_count++;
    if (mock_gpio_fail_next) { mock_gpio_fail_next = 0; return 1; }
    if (mock_gpio_fail_at >= 0 && n == mock_gpio_fail_at) return 1;
    return 0;
}

/* --- output pin: void (*set)(void *ctx, uint8_t level) --- */
/* Matches gpio_led_hal_t, stepper, buzzer, etc. */

__attribute__((unused))
static void mock_gpio_set(void *ctx, uint8_t level)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    if (mock_gpio_should_fail()) return;
    mock_gpio_write_count++;
    mock_gpio_pins[pin] = level;
}

/* --- input pin: uint8_t (*read)(void *ctx) --- */
/* Matches gpio_button_hal_t, rotary_encoder, keypad, etc. */

__attribute__((unused))
static uint8_t mock_gpio_read(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    if (mock_gpio_should_fail()) return 0;
    mock_gpio_read_count++;
    return mock_gpio_pins[pin];
}

/* --- direction control --- */

__attribute__((unused))
static void mock_gpio_set_output(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    mock_gpio_dir[pin] = 1;
}

__attribute__((unused))
static void mock_gpio_set_input(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    mock_gpio_dir[pin] = 0;
}

/* --- delay stubs for drivers needing GPIO + delay --- */

__attribute__((unused))
static void mock_gpio_delay_us(void *ctx, uint32_t us)
{
    (void)ctx; (void)us;
    mock_gpio_delay_count++;
}

__attribute__((unused))
static void mock_gpio_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    mock_gpio_delay_count++;
}

#endif /* MOCK_GPIO_H */
