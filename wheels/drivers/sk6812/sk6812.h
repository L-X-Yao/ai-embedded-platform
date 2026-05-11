// SPDX-License-Identifier: MIT
/*
 * sk6812 — WorldSemi SK6812 RGBW addressable LED driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * 4 bytes per LED in GRB+W order.  Timing protocol similar to WS2812
 * but with an extra white channel.  The timing-critical send_data HAL
 * callback is platform-specific (bit-bang or DMA).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the sk6812_t struct.
 */

#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SK6812_OK        =  0,
    SK6812_ERR_PARAM = -1,
    SK6812_ERR_HAL   = -2,
} sk6812_err_t;

/* ---------- pixel ---------- */

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;
} sk6812_pixel_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    /** Send raw byte stream with protocol timing.  Platform-specific. */
    bool (*send_data)(void *ctx, const uint8_t *data, uint32_t len);
    void *ctx;
} sk6812_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const sk6812_hal_t *hal;
    uint16_t            num_leds;
} sk6812_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Initialize driver. */
sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_hal_t *hal,
                         uint16_t num_leds);

/** Set one pixel in the buffer. Does NOT transmit. */
sk6812_err_t sk6812_set_pixel(sk6812_pixel_t *buf, uint16_t num_leds,
                              uint16_t index,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/** Transmit pixel buffer to the LED strip. */
sk6812_err_t sk6812_show(const sk6812_t *dev, const sk6812_pixel_t *buf,
                         uint16_t count);

/** Fill entire buffer with one color. */
sk6812_err_t sk6812_fill(sk6812_pixel_t *buf, uint16_t num_leds,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/** Clear entire buffer (all off). */
sk6812_err_t sk6812_clear(sk6812_pixel_t *buf, uint16_t num_leds);

#ifdef __cplusplus
}
#endif

#endif /* SK6812_H */
