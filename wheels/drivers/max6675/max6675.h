// SPDX-License-Identifier: MIT
/*
 * max6675 — Maxim MAX6675 K-type thermocouple-to-digital converter
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Read-only SPI device: 16-bit frame, 12-bit temperature (0.25°C/LSB),
 * range 0°C to +1023.75°C. Bit 2 indicates open thermocouple.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MAX6675_H
#define MAX6675_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAX6675_OK         =  0,
    MAX6675_ERR_PARAM  = -1,
    MAX6675_ERR_SPI    = -2,
    MAX6675_ERR_OPEN   = -3,
} max6675_err_t;

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} max6675_spi_t;

typedef struct {
    const max6675_spi_t *spi;
} max6675_t;

/** Initialize. @thread_safe: no @isr_safe: no */
max6675_err_t max6675_init(max6675_t *dev, const max6675_spi_t *spi);

/**
 * Read thermocouple temperature in degrees Celsius.
 * Returns MAX6675_ERR_OPEN if thermocouple is disconnected.
 * @thread_safe: no @isr_safe: no
 */
max6675_err_t max6675_read(max6675_t *dev, float *temp_c);

/** Read raw 16-bit frame for diagnostics. @thread_safe: no @isr_safe: no */
max6675_err_t max6675_read_raw(max6675_t *dev, uint16_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* MAX6675_H */
