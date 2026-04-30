/*
 * max7219 — Maxim MAX7219 LED matrix/7-segment display driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal (injected)
 *
 * SPI interface: 16-bit shift register (address + data).
 * Supports 8 digits/rows, intensity control, BCD/raw mode.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the max7219_t struct.
 */

#ifndef MAX7219_H
#define MAX7219_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAX7219_OK          =  0,
    MAX7219_ERR_SPI     = -1,
    MAX7219_ERR_PARAM   = -2,
} max7219_err_t;

typedef struct {
    bool (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*cs_low)(void *ctx);
    void (*cs_high)(void *ctx);
    void *ctx;
} max7219_spi_t;

typedef struct {
    max7219_spi_t spi;
} max7219_t;

/** @thread_safe: no */
max7219_err_t max7219_init(max7219_t *dev, const max7219_spi_t *spi);

/** Set display brightness level (0-15). @thread_safe: caller_must_lock */
max7219_err_t max7219_set_intensity(max7219_t *dev, uint8_t level);

/** Set number of digits to scan (1-8). @thread_safe: caller_must_lock */
max7219_err_t max7219_set_scan_limit(max7219_t *dev, uint8_t digits);

/** Enter or exit shutdown mode. @thread_safe: caller_must_lock */
max7219_err_t max7219_shutdown(max7219_t *dev, bool shutdown);

/** Set BCD decode mode bitmask for digits. @thread_safe: caller_must_lock */
max7219_err_t max7219_set_decode_mode(max7219_t *dev, uint8_t mask);

/** Write value to a single digit register (0-7). @thread_safe: caller_must_lock */
max7219_err_t max7219_set_digit(max7219_t *dev, uint8_t digit, uint8_t value);

/** Clear all digit registers to zero. @thread_safe: caller_must_lock */
max7219_err_t max7219_clear(max7219_t *dev);

/** Enable or disable display test mode (all segments on). @thread_safe: caller_must_lock */
max7219_err_t max7219_display_test(max7219_t *dev, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MAX7219_H */
