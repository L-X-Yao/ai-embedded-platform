// SPDX-License-Identifier: MIT
/*
 * ina219 — TI INA219 I2C current/voltage/power monitor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Supports configurable bus voltage range (16V/32V), PGA gain (40-320mV),
 * and shunt resistor calibration. All registers are 16-bit big-endian.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INA219_OK         =  0,
    INA219_ERR_I2C    = -1,
    INA219_ERR_PARAM = -2,
} ina219_err_t;

typedef enum {
    INA219_RANGE_16V = 0,   /* Bus voltage range 0-16V */
    INA219_RANGE_32V = 1,   /* Bus voltage range 0-32V */
} ina219_bus_range_t;

typedef enum {
    INA219_GAIN_40MV  = 0,  /* PGA +/-40mV  */
    INA219_GAIN_80MV  = 1,  /* PGA +/-80mV  */
    INA219_GAIN_160MV = 2,  /* PGA +/-160mV */
    INA219_GAIN_320MV = 3,  /* PGA +/-320mV */
} ina219_gain_t;

typedef struct {
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ina219_i2c_t;

typedef struct {
    const ina219_i2c_t *i2c;
    uint8_t  dev_addr;      /* 7-bit I2C address (typically 0x40-0x4F) */
    float    shunt_ohms;    /* shunt resistor value */
    float    current_lsb;   /* current LSB for calibration (A/bit) */
    float    max_current_a; /* maximum expected current (A) */
} ina219_t;

/** Configure device: writes config register (range, gain, 12-bit, continuous)
 *  and calibration register. @thread_safe: no @isr_safe: no */
ina219_err_t ina219_init(ina219_t *dev, const ina219_i2c_t *i2c,
                         uint8_t dev_addr, float shunt_ohms,
                         ina219_bus_range_t range, ina219_gain_t gain);

/** Read bus voltage in volts. @thread_safe: caller_must_lock @isr_safe: no */
ina219_err_t ina219_read_bus_voltage(const ina219_t *dev, float *voltage_v);

/** Read shunt voltage in millivolts. @thread_safe: caller_must_lock @isr_safe: no */
ina219_err_t ina219_read_shunt_voltage(const ina219_t *dev, float *voltage_mv);

/** Read current in milliamps (via calibration register). @thread_safe: caller_must_lock @isr_safe: no */
ina219_err_t ina219_read_current(const ina219_t *dev, float *current_ma);

/** Read power in milliwatts (via calibration register). @thread_safe: caller_must_lock @isr_safe: no */
ina219_err_t ina219_read_power(const ina219_t *dev, float *power_mw);

/** Software reset (writes 0x8000 to config register). @thread_safe: no @isr_safe: no */
ina219_err_t ina219_reset(ina219_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */
