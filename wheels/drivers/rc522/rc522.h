// SPDX-License-Identifier: MIT
/*
 * rc522 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef RC522_H
#define RC522_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    RC522_OK          =  0,
    RC522_ERR_PARAM   = -1,
    RC522_ERR_SPI     = -2,
    RC522_ERR_ID      = -3,
    RC522_ERR_TIMEOUT = -4,
} rc522_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} rc522_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const rc522_spi_t *spi;
} rc522_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
rc522_err_t rc522_init(rc522_t *dev,
                               const rc522_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* RC522_H */
