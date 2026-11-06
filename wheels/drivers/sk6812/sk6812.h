// SPDX-License-Identifier: MIT
/*
 * sk6812 — WorldSemi SK6812 RGBW addressable LED driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Single-wire protocol, 4 bytes per LED in GRBW order.
 * For the 32-bit RGBW parts only. A 24-bit SK6812 is not supported: it would
 * take the W byte as the next LED's G, so every LED after the first is wrong.
 * Timing is platform-specific; HAL provides send_data callback.
 * Caller provides pixel buffer (sk6812_pixel_t array).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the sk6812_t struct.
 */

#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SK6812_OK        =  0,
    SK6812_ERR_PARAM = -1,
    SK6812_ERR_HAL   = -2,
} sk6812_err_t;

/* ---------- pixel (GRBW wire order) ---------- */

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;
} sk6812_pixel_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    bool (*send_data)(void *ctx, const uint8_t *data, size_t len);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} sk6812_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const sk6812_hal_t *hal;
    uint16_t num_leds;
} sk6812_t;

/* ---------- API (all @thread_safe: caller_must_lock) @isr_safe: no ---------- */

/** Initialize driver with HAL and number of LEDs. */
sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_hal_t *hal,
                         uint16_t num_leds);

/** Set a single pixel in the buffer. */
sk6812_err_t sk6812_set_pixel(sk6812_t *dev, sk6812_pixel_t *buf,
                              uint16_t idx,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/** Send pixel buffer to LED strip (send_data + reset delay). */
sk6812_err_t sk6812_show(sk6812_t *dev, const sk6812_pixel_t *buf);

/** Fill entire buffer with one color. */
sk6812_err_t sk6812_fill(sk6812_t *dev, sk6812_pixel_t *buf,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/** Clear entire buffer (all off). */
sk6812_err_t sk6812_clear(sk6812_t *dev, sk6812_pixel_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* SK6812_H */
