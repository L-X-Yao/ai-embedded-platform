// SPDX-License-Identifier: MIT
/*
 * apa102 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
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

/* ---------- device context ---------- */

typedef struct {
    const apa102_spi_t *spi;
} apa102_t;

/* ---------- API ---------- */

/** Initialize and verify chip identity. @thread_safe: no @isr_safe: no */
apa102_err_t apa102_init(apa102_t *dev,
                               const apa102_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* APA102_H */
