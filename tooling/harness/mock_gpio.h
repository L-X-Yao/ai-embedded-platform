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

/* pin direction: 0 = input, 1 = output */
typedef enum { MOCK_GPIO_DIR_INPUT = 0, MOCK_GPIO_DIR_OUTPUT = 1 } mock_gpio_dir_t;

static bool           mock_gpio_pins[MOCK_GPIO_PIN_COUNT];
static mock_gpio_dir_t mock_gpio_dir[MOCK_GPIO_PIN_COUNT];
static int            mock_gpio_write_count;
static int            mock_gpio_read_count;
static int            mock_gpio_fail_next;
static int            mock_gpio_fail_at;
static int            mock_gpio_op_count;
static uint32_t       mock_gpio_delay_count;

/* Build a ctx value that encodes pin index. */
__attribute__((unused))
static void *mock_gpio_make_ctx(uint8_t pin)
{
    return (void *)(uintptr_t)pin;
}

/* Extract pin index from ctx; NULL → pin 0 for backward compatibility. */
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

static bool mock_gpio_should_fail(void)
{
    int n = mock_gpio_op_count++;
    if (mock_gpio_fail_next) { mock_gpio_fail_next = 0; return true; }
    if (mock_gpio_fail_at >= 0 && n == mock_gpio_fail_at) return true;
    return false;
}

__attribute__((unused))
static void mock_gpio_write_pin(void *ctx, bool level)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    if (mock_gpio_should_fail()) return;
    mock_gpio_write_count++;
    mock_gpio_pins[pin] = level;
}

__attribute__((unused))
static bool mock_gpio_read_pin(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    if (mock_gpio_should_fail()) return false;
    mock_gpio_read_count++;
    return mock_gpio_pins[pin];
}

__attribute__((unused))
static void mock_gpio_set_output(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    mock_gpio_dir[pin] = MOCK_GPIO_DIR_OUTPUT;
}

__attribute__((unused))
static void mock_gpio_set_input(void *ctx)
{
    uint8_t pin = mock_gpio_pin_from_ctx(ctx);
    mock_gpio_dir[pin] = MOCK_GPIO_DIR_INPUT;
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
