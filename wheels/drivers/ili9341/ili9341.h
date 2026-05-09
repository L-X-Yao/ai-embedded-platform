// SPDX-License-Identifier: MIT
/*
 * ili9341 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ILI9341_OK          =  0,
    ILI9341_ERR_PARAM   = -1,
    ILI9341_ERR_SPI     = -2,
    ILI9341_ERR_ID      = -3,
    ILI9341_ERR_TIMEOUT = -4,
} ili9341_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} ili9341_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const ili9341_spi_t *spi;
} ili9341_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
ili9341_err_t ili9341_init(ili9341_t *dev,
                               const ili9341_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
