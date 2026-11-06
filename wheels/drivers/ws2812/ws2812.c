// SPDX-License-Identifier: MIT
/*
 * ws2812 — WS2812 addressable RGB LED driver (bit-bang)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "ws2812.h"
#include "contract.h"
#include <stddef.h>

static const uint8_t s_scale5to8_lut[32] = {
      0U,   8U,  16U,  24U,  32U,  41U,  49U,  57U,
     65U,  74U,  82U,  90U,  98U, 106U, 115U, 123U,
    131U, 139U, 148U, 156U, 164U, 172U, 180U, 189U,
    197U, 205U, 213U, 222U, 230U, 238U, 246U, 255U
};

uint8_t ws2812_scale5to8(uint8_t v5)
{
    if (v5 > 31U) {
        v5 = 31U;
    }
    return s_scale5to8_lut[v5];
}

/* The PWM slot for the top bit of value */
static uint16_t top_bit_slot(uint8_t value, const ws2812_timing_t *timing)
{
    return ((value & 0x80U) != 0U) ? timing->t1h_ticks : timing->t0h_ticks;
}

static void encode_byte(uint8_t value, const ws2812_timing_t *timing,
                         uint16_t *dst)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        dst[i] = top_bit_slot(value, timing);
        value <<= 1U;
    }
}

/* A free-running PWM repeats the last compare value in every period that gets
 * no new one, and the LED takes that as an extra bit. A bit period is only 60
 * cycles at 48 MHz, so nothing longer than picking the next slot may run
 * between two bits: not even between two LEDs. */
static bool send_byte(const ws2812_hal_t *hal, const ws2812_timing_t *timing,
                      uint8_t value)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        uint16_t slot = top_bit_slot(value, timing);

        value <<= 1U;
        hal->wait_period(hal->ctx);
        if (!hal->set_compare(hal->ctx, slot))
            return false;
    }
    return true;
}

void ws2812_encode_grb(const ws2812_color_t *color,
                       const ws2812_timing_t *timing,
                       uint16_t *slots_out)
{
    if (!color || !timing || !slots_out) return;
    encode_byte(color->g, timing, &slots_out[0]);
    encode_byte(color->r, timing, &slots_out[8]);
    encode_byte(color->b, timing, &slots_out[16]);
}

static bool reset_latch(const ws2812_hal_t *hal,
                         const ws2812_timing_t *timing)
{
    uint16_t i;

    if (!hal->set_compare(hal->ctx, 0U))
        return false;
    for (i = 0U; i < timing->reset_periods; i++) {
        hal->wait_period(hal->ctx);
    }
    return true;
}

ws2812_err_t ws2812_send(const ws2812_hal_t *hal,
                         const ws2812_timing_t *timing,
                         const ws2812_color_t *colors,
                         uint32_t num_leds)
{
    uint32_t led;

    if (hal == NULL || timing == NULL || colors == NULL || num_leds == 0U) {
        return WS2812_ERR_NULL;
    }
    if (!hal->set_compare || !hal->wait_period || !hal->enter_critical || !hal->exit_critical) {
        return WS2812_ERR_NULL;
    }

    if (!reset_latch(hal, timing))
        return WS2812_ERR_HAL;

    hal->enter_critical(hal->ctx);

    for (led = 0U; led < num_leds; led++) {
        if (!send_byte(hal, timing, colors[led].g) ||
            !send_byte(hal, timing, colors[led].r) ||
            !send_byte(hal, timing, colors[led].b)) {
            hal->exit_critical(hal->ctx);
            return WS2812_ERR_HAL;
        }
    }

    hal->wait_period(hal->ctx);
    if (!hal->set_compare(hal->ctx, 0U)) {
        hal->exit_critical(hal->ctx);
        return WS2812_ERR_HAL;
    }

    hal->exit_critical(hal->ctx);

    return WS2812_OK;
}
