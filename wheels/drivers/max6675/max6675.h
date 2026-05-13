// SPDX-License-Identifier: MIT
/*
 * max6675 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef MAX6675_H
#define MAX6675_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MAX6675_OK          =  0,
    MAX6675_ERR_PARAM   = -1,
    MAX6675_ERR_SPI     = -2,
    MAX6675_ERR_ID      = -3,
    MAX6675_ERR_TIMEOUT = -4,
} max6675_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} max6675_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const max6675_spi_t *spi;
} max6675_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
max6675_err_t max6675_init(max6675_t *dev,
                               const max6675_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* MAX6675_H */
