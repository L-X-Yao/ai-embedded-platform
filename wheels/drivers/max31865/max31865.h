// SPDX-License-Identifier: MIT
/*
 * max31865 — SPI RTD-to-digital converter for PT100/PT1000
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Supports MAX31865 RTD-to-digital converter with Callendar-Van Dusen
 * linearization. Config via SPI; reads 15-bit RTD ratio from registers
 * 0x01-0x02, converts to temperature using Rref injection.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MAX31865_H
#define MAX31865_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MAX31865_OK          =  0,
    MAX31865_ERR_SPI     = -1,
    MAX31865_ERR_PARAM   = -2,
    MAX31865_ERR_FAULT   = -3,
} max31865_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} max31865_spi_t;

/* ---------- wire count ---------- */

typedef enum {
    MAX31865_WIRE_2 = 0,
    MAX31865_WIRE_3 = 1,
    MAX31865_WIRE_4 = 0,  /* same config bit as 2-wire */
} max31865_wire_t;

/* ---------- config ---------- */

typedef struct {
    float             r_ref;   /* reference resistor, e.g. 430.0 for PT100 */
    float             r_nom;   /* nominal RTD resistance at 0C, e.g. 100.0 */
    max31865_wire_t   wire;    /* 2/3/4-wire */
    bool              filter_50hz;  /* true=50Hz, false=60Hz rejection */
} max31865_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    max31865_spi_t spi;
    max31865_cfg_t cfg;
} max31865_t;

/* ---------- fault bits (register 0x07) ---------- */

#define MAX31865_FAULT_HIGHTHRESH   0x80
#define MAX31865_FAULT_LOWTHRESH    0x40
#define MAX31865_FAULT_REFINLOW     0x20
#define MAX31865_FAULT_REFINHIGH    0x10
#define MAX31865_FAULT_RTDLOW       0x08
#define MAX31865_FAULT_OVUV         0x04

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Configure chip and start automatic conversion. */
max31865_err_t max31865_init(max31865_t *dev, const max31865_spi_t *spi,
                             const max31865_cfg_t *cfg);

/** Read temperature in degrees Celsius via Callendar-Van Dusen. */
max31865_err_t max31865_read_temperature(max31865_t *dev, float *temp_c);

/** Read raw 15-bit RTD ADC value (0..32767). */
max31865_err_t max31865_read_rtd_raw(max31865_t *dev, uint16_t *rtd_raw);

/** Read fault status register (0x07). */
max31865_err_t max31865_read_fault(max31865_t *dev, uint8_t *fault);

/** Clear fault status by toggling fault-clear bit in config register. */
max31865_err_t max31865_clear_fault(max31865_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MAX31865_H */
