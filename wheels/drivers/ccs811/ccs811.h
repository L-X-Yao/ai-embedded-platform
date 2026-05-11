// SPDX-License-Identifier: MIT
/*
 * ccs811 — AMS CCS811 eCO2/TVOC gas sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads eCO2 (400-8192 ppm) and
 * TVOC (0-1187 ppb). Supports drive mode configuration and environmental
 * data compensation. Boot sequence: check APP_VALID, start app, set mode.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef CCS811_H
#define CCS811_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CCS811_OK             =  0,
    CCS811_ERR_I2C        = -1,
    CCS811_ERR_ID         = -2,
    CCS811_ERR_PARAM      = -3,
    CCS811_ERR_BOOT       = -4,
    CCS811_ERR_NOT_READY  = -5,
} ccs811_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} ccs811_i2c_t;

typedef enum {
    CCS811_MODE_IDLE   = 0,
    CCS811_MODE_1SEC   = 1,
    CCS811_MODE_10SEC  = 2,
    CCS811_MODE_60SEC  = 3,
    CCS811_MODE_250MS  = 4,
} ccs811_drive_mode_t;

typedef struct {
    uint8_t             addr;
    ccs811_drive_mode_t mode;
} ccs811_cfg_t;

typedef struct {
    uint16_t eco2_ppm;
    uint16_t tvoc_ppb;
} ccs811_data_t;

typedef struct {
    const ccs811_i2c_t *i2c;
    ccs811_cfg_t cfg;
} ccs811_t;

#define CCS811_ADDR_PRIMARY  0x5A
#define CCS811_ADDR_ALT      0x5B
#define CCS811_HW_ID         0x81

/** @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_read(const ccs811_t *dev, ccs811_data_t *out);

/** @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_set_mode(ccs811_t *dev, ccs811_drive_mode_t mode);

/** Set environmental data for compensation. humidity in 0.5% steps,
 *  temperature in 0.5C steps offset by +25C.
 *  @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_set_env_data(const ccs811_t *dev, uint16_t humidity,
                                 uint16_t temperature);

#ifdef __cplusplus
}
#endif

#endif /* CCS811_H */
