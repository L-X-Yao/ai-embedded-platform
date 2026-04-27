#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

/* ---------- error codes ---------- */

typedef enum {
    WS2812_OK = 0,
    WS2812_ERR_NULL,
    WS2812_ERR_HAL,
} ws2812_err_t;

/* ---------- timing configuration (clock-dependent) ---------- */

typedef struct {
    uint16_t period_ticks;
    uint16_t t0h_ticks;
    uint16_t t1h_ticks;
    uint16_t reset_periods;
} ws2812_timing_t;

/* ---------- PWM HAL (caller provides) ---------- */

typedef struct {
    void (*set_compare)(void *ctx, uint16_t value);
    void (*wait_period)(void *ctx);
    void (*enter_critical)(void *ctx);
    void (*exit_critical)(void *ctx);
    void *ctx;
} ws2812_hal_t;

/* ---------- color ---------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

/* ---------- API ---------- */

ws2812_err_t ws2812_send(const ws2812_hal_t *hal,
                         const ws2812_timing_t *timing,
                         const ws2812_color_t *colors,
                         uint32_t num_leds);

void ws2812_encode_grb(const ws2812_color_t *color,
                       const ws2812_timing_t *timing,
                       uint16_t *slots_out);

uint8_t ws2812_scale5to8(uint8_t v5);

#endif /* WS2812_H */
