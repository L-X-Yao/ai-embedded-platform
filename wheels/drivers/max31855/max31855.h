// SPDX-License-Identifier: MIT
/*
 * max31855 — MAX31855 SPI thermocouple-to-digital converter driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal (injected)
 *
 * SPI interface injected via callbacks. Reads 32-bit frames containing
 * 14-bit thermocouple temperature and 12-bit internal reference temperature.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MAX31855_H
#define MAX31855_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MAX31855_OK            =  0,
    MAX31855_ERR_SPI       = -1,
    MAX31855_ERR_PARAM    = -2,
    MAX31855_ERR_OPEN      = -3,
    MAX31855_ERR_SHORT_GND = -4,
    MAX31855_ERR_SHORT_VCC = -5,
} max31855_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} max31855_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const max31855_spi_t *spi;
} max31855_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize device context. @thread_safe: no */
max31855_err_t max31855_init(max31855_t *dev, const max31855_spi_t *spi);

/** Read thermocouple and internal temperatures in Celsius.
 *  Either output pointer may be NULL if not needed.
 *  @thread_safe: caller_must_lock */
max31855_err_t max31855_read(max31855_t *dev,
                             float *thermocouple_c, float *internal_c);

/** Read raw 32-bit SPI frame. @thread_safe: caller_must_lock */
max31855_err_t max31855_read_raw(max31855_t *dev, uint32_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* MAX31855_H */
