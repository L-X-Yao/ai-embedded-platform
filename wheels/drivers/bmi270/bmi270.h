// SPDX-License-Identifier: MIT
/*
 * bmi270 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef BMI270_H
#define BMI270_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BMI270_OK          =  0,
    BMI270_ERR_PARAM   = -1,
    BMI270_ERR_SPI     = -2,
    BMI270_ERR_ID      = -3,
    BMI270_ERR_TIMEOUT = -4,
} bmi270_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} bmi270_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const bmi270_spi_t *spi;
} bmi270_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_init(bmi270_t *dev,
                               const bmi270_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* BMI270_H */
