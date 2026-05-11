// SPDX-License-Identifier: MIT
/*
 * cc1101 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CC1101_OK          =  0,
    CC1101_ERR_PARAM   = -1,
    CC1101_ERR_SPI     = -2,
    CC1101_ERR_ID      = -3,
    CC1101_ERR_TIMEOUT = -4,
} cc1101_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} cc1101_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const cc1101_spi_t *spi;
} cc1101_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
cc1101_err_t cc1101_init(cc1101_t *dev,
                               const cc1101_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* CC1101_H */
