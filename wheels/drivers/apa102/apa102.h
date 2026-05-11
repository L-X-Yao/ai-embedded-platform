// SPDX-License-Identifier: MIT
/*
 * apa102 — APA102 (DotStar) SPI-based addressable RGB LED driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI protocol: start frame (4x 0x00) + N LED frames + end frame.
 * Each LED frame: [0xE0|brightness(0-31)] [blue] [green] [red].
 * End frame: ceil(N/2) bytes of 0xFF (or minimum 4 bytes).
 * Pixel buffer is caller-provided. Pure C99, no heap.
 */

#ifndef APA102_H
#define APA102_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    APA102_OK          =  0,
    APA102_ERR_PARAM   = -1,
    APA102_ERR_SPI     = -2,
    APA102_ERR_ID      = -3,
    APA102_ERR_TIMEOUT = -4,
} apa102_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} apa102_spi_t;

/* ---------- pixel type ---------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;  /* 0-31 */
} apa102_pixel_t;

/* ---------- device context ---------- */

typedef struct {
    const apa102_spi_t *spi;
    uint16_t            num_leds;
} apa102_t;

/* ---------- API ---------- */

/** Initialize the APA102 driver.
 *  @param dev       Device context (caller-owned)
 *  @param spi       SPI HAL callbacks
 *  @param num_leds  Number of LEDs in the strip
 *  @thread_safe: no @isr_safe: no */
apa102_err_t apa102_init(apa102_t *dev, const apa102_spi_t *spi,
                         uint16_t num_leds);

/** Set a single pixel in the caller's buffer.
 *  @param buf        Pixel buffer (caller-owned, length >= num_leds)
 *  @param num_leds   Buffer capacity
 *  @param index      LED index (0-based)
 *  @param r,g,b      Color components (0-255)
 *  @param brightness Global brightness (0-31)
 *  @thread_safe: no @isr_safe: yes */
apa102_err_t apa102_set_pixel(apa102_pixel_t *buf, uint16_t num_leds,
                              uint16_t index, uint8_t r, uint8_t g,
                              uint8_t b, uint8_t brightness);

/** Flush the pixel buffer to the LED strip via SPI.
 *  @param dev  Device context
 *  @param buf  Pixel buffer to send
 *  @thread_safe: no @isr_safe: no */
apa102_err_t apa102_show(apa102_t *dev, const apa102_pixel_t *buf);

/** Set all pixels off (clear). Convenience wrapper.
 *  @thread_safe: no @isr_safe: yes */
void apa102_clear(apa102_pixel_t *buf, uint16_t num_leds);

#ifdef __cplusplus
}
#endif

#endif /* APA102_H */
