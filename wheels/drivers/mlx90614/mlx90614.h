// SPDX-License-Identifier: MIT
/*
 * mlx90614 — Melexis MLX90614 non-contact infrared temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * SMBus protocol via command-based I2C (write+read, not register-based).
 * Temperature conversion: raw * 0.02 - 273.15 (Kelvin to Celsius).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MLX90614_H
#define MLX90614_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MLX90614_OK        =  0,
    MLX90614_ERR_I2C   = -1,
    MLX90614_ERR_PARAM = -2,
    MLX90614_ERR_RANGE = -3,
} mlx90614_err_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);  /* required for EEPROM writes */
    void *ctx;
} mlx90614_i2c_t;

typedef struct {
    uint8_t addr;
} mlx90614_cfg_t;

typedef struct {
    const mlx90614_i2c_t *i2c;
    mlx90614_cfg_t cfg;
} mlx90614_t;

#define MLX90614_ADDR_DEFAULT  0x5Au

/** @thread_safe: no @isr_safe: no */
mlx90614_err_t mlx90614_init(mlx90614_t *dev, const mlx90614_i2c_t *i2c,
                             const mlx90614_cfg_t *cfg);

/** Read object (IR target) temperature in Celsius. @thread_safe: no @isr_safe: yes */
mlx90614_err_t mlx90614_read_object_temp(mlx90614_t *dev, float *temp_c);

/** Read ambient (sensor die) temperature in Celsius. @thread_safe: no @isr_safe: yes */
mlx90614_err_t mlx90614_read_ambient_temp(mlx90614_t *dev, float *temp_c);

/** Read emissivity setting (0.1 .. 1.0). @thread_safe: no @isr_safe: no */
mlx90614_err_t mlx90614_read_emissivity(mlx90614_t *dev, float *emissivity);

/** Set emissivity (0.1 .. 1.0). Writes to EEPROM — limit write cycles.
 *  @thread_safe: no @isr_safe: no */
mlx90614_err_t mlx90614_set_emissivity(mlx90614_t *dev, float emissivity);

#ifdef __cplusplus
}
#endif

#endif /* MLX90614_H */
