/*
 * mock_time.h — shared delay and tick mock for wheel tests
 *
 * Header-only. Provides delay callbacks (ms/us) with call counting
 * and an injectable tick source for time-dependent wheels.
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "mock_time.h"
 *
 *   static const my_driver_i2c_t hal = {
 *       .delay_ms = mock_delay_ms,
 *       .ctx      = NULL,
 *   };
 *
 *   // For tick-driven wheels (soft_timer, timeout, etc.):
 *   mock_time_reset();
 *   mock_time_advance(100);  // advance 100 ms
 *   uint32_t now = mock_time_get_ms();
 */

#ifndef MOCK_TIME_H
#define MOCK_TIME_H

#include <stdint.h>

static uint32_t mock_time_ms;
static uint32_t mock_time_delay_calls;
static uint32_t mock_time_delay_total_ms;
static uint32_t mock_time_delay_last_ms;
static uint32_t mock_time_delay_us_calls;
static uint32_t mock_time_delay_total_us;

__attribute__((unused))
static void mock_time_reset(void)
{
    mock_time_ms             = 0;
    mock_time_delay_calls    = 0;
    mock_time_delay_total_ms = 0;
    mock_time_delay_last_ms  = 0;
    mock_time_delay_us_calls = 0;
    mock_time_delay_total_us = 0;
}

__attribute__((unused))
static void mock_time_advance(uint32_t ms)
{
    mock_time_ms += ms;
}

__attribute__((unused))
static uint32_t mock_time_get_ms(void)
{
    return mock_time_ms;
}

/* --- delay callbacks matching common HAL signatures -------------------- */

/* void (*delay_ms)(void *ctx, uint32_t ms) — dominant pattern (28+ drivers) */
__attribute__((unused))
static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_time_delay_calls++;
    mock_time_delay_total_ms += ms;
    mock_time_delay_last_ms = ms;
    mock_time_ms += ms;
}

/* void (*delay_ms)(void *ctx, uint16_t ms) — aht20, sht40, buzzer */
__attribute__((unused))
static void mock_delay_ms_u16(void *ctx, uint16_t ms)
{
    mock_delay_ms(ctx, (uint32_t)ms);
}

/* void (*delay_us)(void *ctx, uint32_t us) — soft_i2c, onewire, dht11 */
__attribute__((unused))
static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_time_delay_us_calls++;
    mock_time_delay_total_us += us;
}

/* uint32_t (*get_ms)(void *ctx) — tick source for time-dependent modules */
__attribute__((unused))
static uint32_t mock_get_ms(void *ctx)
{
    (void)ctx;
    return mock_time_ms;
}

#endif /* MOCK_TIME_H */
