/*
 * hmc5883l — Honeywell HMC5883L 3-axis digital magnetometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * I2C register-based interface. Fixed address 0x1E.
 * Outputs in raw counts or milligauss (configurable gain).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the hmc5883l_t struct.
 */

#ifndef HMC5883L_H
#define HMC5883L_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HMC5883L_OK          =  0,
    HMC5883L_ERR_I2C     = -1,
    HMC5883L_ERR_ID      = -2,
    HMC5883L_ERR_PARAM   = -3,
} hmc5883l_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} hmc5883l_i2c_t;

typedef enum {
    HMC5883L_GAIN_1370 = 0,
    HMC5883L_GAIN_1090 = 1,
    HMC5883L_GAIN_820  = 2,
    HMC5883L_GAIN_660  = 3,
    HMC5883L_GAIN_440  = 4,
    HMC5883L_GAIN_390  = 5,
    HMC5883L_GAIN_330  = 6,
    HMC5883L_GAIN_230  = 7,
} hmc5883l_gain_t;

typedef enum {
    HMC5883L_RATE_0_75 = 0,
    HMC5883L_RATE_1_5  = 1,
    HMC5883L_RATE_3    = 2,
    HMC5883L_RATE_7_5  = 3,
    HMC5883L_RATE_15   = 4,
    HMC5883L_RATE_30   = 5,
    HMC5883L_RATE_75   = 6,
} hmc5883l_rate_t;

typedef struct {
    hmc5883l_gain_t gain;
    hmc5883l_rate_t rate;
} hmc5883l_cfg_t;

typedef struct {
    hmc5883l_i2c_t i2c;
    hmc5883l_cfg_t cfg;
    float          scale;
} hmc5883l_t;

typedef struct {
    int16_t x, y, z;
} hmc5883l_raw_t;

typedef struct {
    float x, y, z;
} hmc5883l_data_t;

#define HMC5883L_ADDR 0x1E

/** @thread_safe: no */
hmc5883l_err_t hmc5883l_init(hmc5883l_t *dev, const hmc5883l_i2c_t *i2c,
                             const hmc5883l_cfg_t *cfg);

/** Read raw 16-bit magnetometer values for X, Y, Z axes. @thread_safe: caller_must_lock */
hmc5883l_err_t hmc5883l_read_raw(hmc5883l_t *dev, hmc5883l_raw_t *raw);

/** Read magnetometer values scaled to gauss. @thread_safe: caller_must_lock */
hmc5883l_err_t hmc5883l_read_gauss(hmc5883l_t *dev, hmc5883l_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HMC5883L_H */
