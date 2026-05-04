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
#include <stddef.h>

#define WS2812_BITS_PER_LED  24U

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

static void encode_byte(uint8_t value, const ws2812_timing_t *timing,
                         uint16_t *dst)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        if ((value & 0x80U) != 0U) {
            dst[i] = timing->t1h_ticks;
        } else {
            dst[i] = timing->t0h_ticks;
        }
        value <<= 1U;
    }
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
    uint16_t slots[WS2812_BITS_PER_LED];
    uint32_t led;
    uint32_t bit;

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
        (void)ws2812_encode_grb(&colors[led], timing, slots);

        hal->wait_period(hal->ctx);
        if (!hal->set_compare(hal->ctx, slots[0])) {
            hal->exit_critical(hal->ctx);
            return WS2812_ERR_HAL;
        }

        for (bit = 1U; bit < WS2812_BITS_PER_LED; bit++) {
            hal->wait_period(hal->ctx);
            if (!hal->set_compare(hal->ctx, slots[bit])) {
                hal->exit_critical(hal->ctx);
                return WS2812_ERR_HAL;
            }
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
