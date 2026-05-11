// SPDX-License-Identifier: MIT
/*
 * sx1262 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SX1262_OK          =  0,
    SX1262_ERR_PARAM   = -1,
    SX1262_ERR_SPI     = -2,
    SX1262_ERR_ID      = -3,
    SX1262_ERR_TIMEOUT = -4,
} sx1262_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} sx1262_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const sx1262_spi_t *spi;
} sx1262_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
sx1262_err_t sx1262_init(sx1262_t *dev,
                               const sx1262_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* SX1262_H */
