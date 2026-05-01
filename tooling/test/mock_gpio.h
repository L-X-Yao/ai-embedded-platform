/*
 * mock_gpio.h — shared GPIO mock for L1 driver tests
 *
 * Header-only. Provides pin state tracking for GPIO-based drivers
 * (buttons, LEDs, stepper, keypad, rotary encoder, etc.)
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "mock_gpio.h"
 *   #include "my_driver.h"
 *
 *   static void mock_reset(void) {
 *       mock_gpio_reset();
 *       mock_gpio_pins[0] = true;  // pin 0 starts high
 *   }
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef MOCK_GPIO_PIN_COUNT
#define MOCK_GPIO_PIN_COUNT 16
#endif

static bool     mock_gpio_pins[MOCK_GPIO_PIN_COUNT];
static int      mock_gpio_write_count;
static int      mock_gpio_read_count;
static int      mock_gpio_fail_next;
static uint32_t mock_gpio_delay_count;

static void mock_gpio_reset(void)
{
    memset(mock_gpio_pins, 0, sizeof(mock_gpio_pins));
    mock_gpio_write_count = 0;
    mock_gpio_read_count  = 0;
    mock_gpio_fail_next   = 0;
    mock_gpio_delay_count = 0;
}

__attribute__((unused))
static void mock_gpio_write_pin(void *ctx, bool level)
{
    (void)ctx;
    mock_gpio_write_count++;
    mock_gpio_pins[0] = level;
}

__attribute__((unused))
static bool mock_gpio_read_pin(void *ctx)
{
    (void)ctx;
    mock_gpio_read_count++;
    return mock_gpio_pins[0];
}

__attribute__((unused))
static void mock_gpio_set_output(void *ctx)
{
    (void)ctx;
}

__attribute__((unused))
static void mock_gpio_set_input(void *ctx)
{
    (void)ctx;
}

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
