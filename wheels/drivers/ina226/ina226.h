// SPDX-License-Identifier: MIT
/*
 * ina226 — TI INA226 precision I2C current/power monitor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * All registers are 16-bit big-endian. Supports configurable averaging,
 * conversion time, and shunt resistance calibration. Verifies manufacturer
 * and die IDs (0x5449 / 0x2260).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef INA226_H
#define INA226_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    INA226_OK        =  0,
    INA226_ERR_I2C   = -1,
    INA226_ERR_ID    = -2,
    INA226_ERR_PARAM = -3,
} ina226_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ina226_i2c_t;

/* ---------- constants ---------- */

#define INA226_ADDR_DEFAULT  0x40
#define INA226_MFR_ID        0x5449u
#define INA226_DIE_ID        0x2260u

/* ---------- averaging mode ---------- */

typedef enum {
    INA226_AVG_1    = 0,
    INA226_AVG_4    = 1,
    INA226_AVG_16   = 2,
    INA226_AVG_64   = 3,
    INA226_AVG_128  = 4,
    INA226_AVG_256  = 5,
    INA226_AVG_512  = 6,
    INA226_AVG_1024 = 7,
} ina226_avg_t;

/* ---------- conversion time ---------- */

typedef enum {
    INA226_CT_140US  = 0,
    INA226_CT_204US  = 1,
    INA226_CT_332US  = 2,
    INA226_CT_588US  = 3,
    INA226_CT_1100US = 4,   /* default */
    INA226_CT_2116US = 5,
    INA226_CT_4156US = 6,
    INA226_CT_8244US = 7,
} ina226_conv_time_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t              addr;               /* 7-bit I2C address */
    uint16_t             shunt_resistance_mohm; /* shunt resistor in milliohms */
    uint16_t             max_current_ma;     /* max expected current in mA */
    ina226_avg_t         averaging;
    ina226_conv_time_t   bus_conv_time;
    ina226_conv_time_t   shunt_conv_time;
} ina226_cfg_t;

/* ---------- measurement output ---------- */

typedef struct {
    int32_t  shunt_voltage_uv;   /* shunt voltage in microvolts */
    uint32_t bus_voltage_mv;     /* bus voltage in millivolts */
    int32_t  current_ma;         /* current in milliamps (signed) */
    uint32_t power_mw;           /* power in milliwatts */
} ina226_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ina226_i2c_t *i2c;
    ina226_cfg_t  cfg;
    float         current_lsb;   /* A/bit for calibration */
} ina226_t;

/* ---------- API ---------- */

/** Initialize device: verify IDs, write config and calibration registers.
 *  @thread_safe: no @isr_safe: no */
ina226_err_t ina226_init(ina226_t *dev, const ina226_i2c_t *i2c,
                         const ina226_cfg_t *cfg);

/** Read all measurements (shunt voltage, bus voltage, current, power).
 *  @thread_safe: caller_must_lock @isr_safe: no */
ina226_err_t ina226_read(ina226_t *dev, ina226_data_t *out);

/** Software reset (sets RST bit in config register).
 *  @thread_safe: no @isr_safe: no */
ina226_err_t ina226_reset(ina226_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* INA226_H */
