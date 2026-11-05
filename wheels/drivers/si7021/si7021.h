// SPDX-License-Identifier: MIT
/*
 * si7021 — Si7021/HTU21D/SHT21 I2C temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command-based I2C with no-hold measurement mode, CRC-8 protected
 * responses, configurable resolution.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SI7021_H
#define SI7021_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SI7021_OK         =  0,
    SI7021_ERR_I2C    = -1,
    SI7021_ERR_PARAM = -2,
    SI7021_ERR_CRC    = -3,
} si7021_err_t;

typedef enum {
    SI7021_RES_14BIT_12BIT = 0,  /* 14-bit temp, 12-bit RH (default) */
    SI7021_RES_12BIT_8BIT  = 1,
    SI7021_RES_13BIT_10BIT = 2,
    SI7021_RES_11BIT_11BIT = 3,
} si7021_resolution_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} si7021_i2c_t;

typedef struct {
    const si7021_i2c_t *i2c;
} si7021_t;

#define SI7021_DEV_ADDR  0x40u

/** Initialize device context. All of write, read and delay_ms are required:
 *  a measurement waits on delay_ms, because the sensor NACKs the read until
 *  the conversion ends. Returns SI7021_ERR_PARAM if one is missing.
 *  @thread_safe: no @isr_safe: no */
si7021_err_t si7021_init(si7021_t *dev, const si7021_i2c_t *i2c);

/** Read relative humidity (%). @thread_safe: no @isr_safe: yes */
si7021_err_t si7021_read_humidity(si7021_t *dev, float *rh);

/** Read temperature (deg C). @thread_safe: no @isr_safe: yes */
si7021_err_t si7021_read_temperature(si7021_t *dev, float *temp_c);

/** Read both: humidity first, then temperature from previous RH measurement. @thread_safe: no @isr_safe: no */
si7021_err_t si7021_read_both(si7021_t *dev, float *rh, float *temp_c);

/** Set measurement resolution. @thread_safe: no @isr_safe: no */
si7021_err_t si7021_set_resolution(si7021_t *dev, si7021_resolution_t res);

/** Software reset. Waits 15 ms for the sensor to be ready again.
 *  @thread_safe: no @isr_safe: no */
si7021_err_t si7021_reset(si7021_t *dev);

/** Read 64-bit electronic serial number (two 32-bit halves). @thread_safe: no @isr_safe: no */
si7021_err_t si7021_read_serial(si7021_t *dev, uint32_t *serial_a, uint32_t *serial_b);

/** CRC-8 used by Si7021 (poly 0x131, init 0x00). */
uint8_t si7021_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SI7021_H */
