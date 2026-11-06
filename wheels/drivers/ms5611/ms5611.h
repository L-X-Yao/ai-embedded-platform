// SPDX-License-Identifier: MIT
/*
 * ms5611 — TE Connectivity MS5611 precision barometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command-based I2C protocol (not standard register reads). PROM holds
 * factory calibration (C1-C6). Compensation algorithm from MS5611-01BA03
 * datasheet including 2nd-order for low temperatures.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MS5611_H
#define MS5611_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MS5611_OK           =  0,
    MS5611_ERR_I2C      = -1,
    MS5611_ERR_PARAM    = -2,
    MS5611_ERR_CRC      = -3,
} ms5611_err_t;

typedef struct {
    /* Unused and may be NULL: a command is one byte, so it goes out through
     * write_raw. Kept so that existing initialisers still build. */
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required: reads must wait for conversions */
    void *ctx;
    /* Required. Puts exactly data[0..len-1] on the bus after the address,
     * then STOP. ms5611_init returns MS5611_ERR_PARAM without it. */
    bool (*write_raw)(void *ctx, uint8_t dev_addr, const uint8_t *data,
                      uint16_t len);
} ms5611_i2c_t;

typedef enum {
    MS5611_OSR_256  = 0,
    MS5611_OSR_512  = 1,
    MS5611_OSR_1024 = 2,
    MS5611_OSR_2048 = 3,
    MS5611_OSR_4096 = 4,
} ms5611_osr_t;

typedef struct {
    uint8_t      addr;
    ms5611_osr_t osr;
} ms5611_cfg_t;

typedef struct {
    uint16_t c1;  /* pressure sensitivity */
    uint16_t c2;  /* pressure offset */
    uint16_t c3;  /* temp coeff of pressure sensitivity */
    uint16_t c4;  /* temp coeff of pressure offset */
    uint16_t c5;  /* reference temperature */
    uint16_t c6;  /* temp coeff of temperature */
} ms5611_calib_t;

typedef struct {
    const ms5611_i2c_t *i2c;
    ms5611_cfg_t   cfg;
    ms5611_calib_t calib;
} ms5611_t;

typedef struct {
    float temperature_c;
    float pressure_pa;
} ms5611_data_t;

#define MS5611_ADDR_CSB_LOW  0x77
#define MS5611_ADDR_CSB_HIGH 0x76

/** @thread_safe: no @isr_safe: no */
ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out);

/** @thread_safe: no @isr_safe: no */
ms5611_err_t ms5611_reset(ms5611_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MS5611_H */
