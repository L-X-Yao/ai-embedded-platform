// SPDX-License-Identifier: MIT
/*
 * sht30 — Sensirion SHT30 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. SHT30 uses 16-bit command words
 * (not register-based) with CRC-8 protected responses.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHT30_OK        =  0,
    SHT30_ERR_I2C   = -1,
    SHT30_ERR_CRC   = -2,
    SHT30_ERR_PARAM = -3,
} sht30_err_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint16_t ms);
    void *ctx;
} sht30_i2c_t;

typedef struct {
    uint8_t     addr;
} sht30_cfg_t;

typedef struct {
    sht30_i2c_t i2c;
    sht30_cfg_t cfg;
} sht30_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
} sht30_data_t;

#define SHT30_ADDR_A  0x44
#define SHT30_ADDR_B  0x45

/** @thread_safe: no */
sht30_err_t sht30_init(sht30_t *dev, const sht30_i2c_t *i2c,
                       const sht30_cfg_t *cfg);

/** @thread_safe: no */
sht30_err_t sht30_read(sht30_t *dev, sht30_data_t *out);

/** @thread_safe: no */
sht30_err_t sht30_reset(sht30_t *dev);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t sht30_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SHT30_H */
