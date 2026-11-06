// SPDX-License-Identifier: MIT
/*
 * scd40 — Sensirion SCD40/SCD41 NDIR CO2 + temperature + humidity sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. SCD40 uses 16-bit command words
 * (not register-based) with CRC-8 protected data (poly 0x31, init 0xFF).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SCD40_H
#define SCD40_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SCD40_OK        =  0,
    SCD40_ERR_I2C   = -1,
    SCD40_ERR_CRC   = -2,
    SCD40_ERR_PARAM = -3,
    SCD40_ERR_BUSY  = -4,   /* idle mode only: a periodic measurement runs */
} scd40_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required: the sensor NACKs early reads */
    void *ctx;
} scd40_i2c_t;

/* ---------- constants ---------- */

#define SCD40_DEV_ADDR        0x62u
#define SCD40_ALTITUDE_MAX_M  3000u

/* ---------- data types ---------- */

typedef struct {
    uint16_t co2_ppm;
    float    temperature_c;
    float    humidity_pct;
} scd40_data_t;

/* ---------- device context ---------- */

typedef struct {
    const scd40_i2c_t *i2c;
    uint8_t addr;
    bool    periodic;   /* a periodic measurement was started and not stopped */
} scd40_t;

/* ---------- API ---------- */

/** Configure device context. @thread_safe: no @isr_safe: no */
scd40_err_t scd40_init(scd40_t *dev, const scd40_i2c_t *i2c);

/** Start periodic measurement (5 s interval). Returns SCD40_ERR_BUSY if one is
 *  already running. @thread_safe: no @isr_safe: no */
scd40_err_t scd40_start_periodic(scd40_t *dev);

/** Stop periodic measurement. Blocks 500 ms, the time the sensor stays
 *  silent after this command. @thread_safe: no @isr_safe: no */
scd40_err_t scd40_stop_periodic(scd40_t *dev);

/** Read CO2/temp/humidity. Call after data_ready returns true.
 *  @thread_safe: caller_must_lock @isr_safe: no */
scd40_err_t scd40_read(scd40_t *dev, scd40_data_t *out);

/** Check if new measurement data is available.
 *  @thread_safe: caller_must_lock @isr_safe: no */
scd40_err_t scd40_data_ready(scd40_t *dev, bool *ready);

/** Read 48-bit unique serial number (3 x uint16). Idle mode only: returns
 *  SCD40_ERR_BUSY during a periodic measurement. @thread_safe: no @isr_safe: no */
scd40_err_t scd40_get_serial(scd40_t *dev, uint16_t serial[3]);

/** Set altitude compensation, 0 to SCD40_ALTITUDE_MAX_M meters. Idle mode only:
 *  returns SCD40_ERR_BUSY during a periodic measurement.
 *  @thread_safe: no @isr_safe: no */
scd40_err_t scd40_set_altitude(scd40_t *dev, uint16_t meters);

/** Soft reset: reinit, reloads user settings from EEPROM, takes 30 ms.
 *  Idle mode only: call scd40_stop_periodic first, or this returns
 *  SCD40_ERR_BUSY. @thread_safe: no @isr_safe: no */
scd40_err_t scd40_perform_reset(scd40_t *dev);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t scd40_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SCD40_H */
