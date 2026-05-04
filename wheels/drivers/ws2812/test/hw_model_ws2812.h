// SPDX-License-Identifier: MIT
/*
 * hw_model_ws2812.h — Signal decoder model for WS2812 addressable LED strip
 *
 * NOT a register model. The WS2812 driver outputs PWM compare values
 * via HAL callbacks. This model decodes the GRB-encoded PWM stream
 * back into per-LED RGB values for verification.
 *
 * The driver sends 24 PWM slots per LED (8 bits G, 8 bits R, 8 bits B),
 * MSB first. Each slot is either t0h_ticks (bit=0) or t1h_ticks (bit=1).
 * The model captures set_compare calls inside the critical section and
 * decodes them.
 */

#ifndef HW_MODEL_WS2812_H
#define HW_MODEL_WS2812_H

#include "ws2812.h"
#include <string.h>

#define WS2812_MODEL_MAX_LEDS  64

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_model_rgb_t;

typedef struct {
    /* decoded output */
    ws2812_model_rgb_t leds[WS2812_MODEL_MAX_LEDS];
    uint32_t           led_count;

    /* capture state */
    uint16_t slots[WS2812_MODEL_MAX_LEDS * 24];
    uint32_t slot_count;
    int      in_critical;

    /* timing thresholds for decode */
    uint16_t t0h_ticks;
    uint16_t t1h_ticks;
    uint16_t threshold;  /* midpoint for 0/1 decision */
} ws2812_model_t;

static void ws2812_model_init(ws2812_model_t *m,
                               const ws2812_timing_t *timing)
{
    memset(m, 0, sizeof(*m));
    m->t0h_ticks = timing->t0h_ticks;
    m->t1h_ticks = timing->t1h_ticks;
    m->threshold = (uint16_t)((timing->t0h_ticks + timing->t1h_ticks) / 2);
}

/* --- HAL callbacks wired to model --- */

static bool ws2812_model_set_compare(void *ctx, uint16_t value)
{
    ws2812_model_t *m = (ws2812_model_t *)ctx;
    if (m->in_critical && value != 0) {
        if (m->slot_count < WS2812_MODEL_MAX_LEDS * 24) {
            m->slots[m->slot_count++] = value;
        }
    }
    return true;
}

static void ws2812_model_wait_period(void *ctx) { (void)ctx; }

static void ws2812_model_enter_critical(void *ctx)
{
    ws2812_model_t *m = (ws2812_model_t *)ctx;
    m->in_critical = 1;
}

static void ws2812_model_exit_critical(void *ctx)
{
    ws2812_model_t *m = (ws2812_model_t *)ctx;
    m->in_critical = 0;
}

static ws2812_hal_t ws2812_model_hal(ws2812_model_t *m)
{
    ws2812_hal_t hal;
    hal.set_compare    = ws2812_model_set_compare;
    hal.wait_period    = ws2812_model_wait_period;
    hal.enter_critical = ws2812_model_enter_critical;
    hal.exit_critical  = ws2812_model_exit_critical;
    hal.ctx            = m;
    return hal;
}

/* --- Decode captured PWM slots into RGB values --- */

static uint8_t ws2812_model_decode_byte(const ws2812_model_t *m,
                                         const uint16_t *slots)
{
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        val <<= 1;
        if (slots[i] > m->threshold) {
            val |= 1;
        }
    }
    return val;
}

static void ws2812_model_decode(ws2812_model_t *m)
{
    m->led_count = m->slot_count / 24;
    if (m->led_count > WS2812_MODEL_MAX_LEDS) {
        m->led_count = WS2812_MODEL_MAX_LEDS;
    }
    for (uint32_t i = 0; i < m->led_count; i++) {
        const uint16_t *base = &m->slots[i * 24];
        /* WS2812 sends GRB: first 8 bits = Green, next 8 = Red, last 8 = Blue */
        m->leds[i].g = ws2812_model_decode_byte(m, &base[0]);
        m->leds[i].r = ws2812_model_decode_byte(m, &base[8]);
        m->leds[i].b = ws2812_model_decode_byte(m, &base[16]);
    }
}

/* --- Query API --- */

static ws2812_model_rgb_t ws2812_model_get_led(const ws2812_model_t *m,
                                                 uint32_t index)
{
    if (index < m->led_count) {
        return m->leds[index];
    }
    ws2812_model_rgb_t black = {0, 0, 0};
    return black;
}

static uint32_t ws2812_model_get_led_count(const ws2812_model_t *m)
{
    return m->led_count;
}

/* Convenience: send colors through driver, then decode */
static ws2812_err_t ws2812_model_send_and_decode(
    ws2812_model_t *m,
    const ws2812_timing_t *timing,
    const ws2812_color_t *colors,
    uint32_t num_leds)
{
    ws2812_hal_t hal = ws2812_model_hal(m);
    ws2812_err_t err = ws2812_send(&hal, timing, colors, num_leds);
    if (err == WS2812_OK) {
        ws2812_model_decode(m);
    }
    return err;
}

#endif /* HW_MODEL_WS2812_H */
