// SPDX-License-Identifier: MIT
/*
 * sgp40 — Sensirion SGP40 VOC raw signal sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 *
 * SGP40 uses command-based I2C with CRC-8 protection (poly 0x31, init 0xFF).
 * Outputs raw VOC signal (SRAW, 0-65535). Supports humidity/temperature
 * compensation. Self-test command for sensor validation.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SGP40_H
#define SGP40_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SGP40_OK           =  0,
    SGP40_ERR_I2C      = -1,
    SGP40_ERR_CRC      = -2,
    SGP40_ERR_PARAM    = -3,
    SGP40_ERR_SELFTEST = -4,
} sgp40_err_t;

/* Raw write/read I2C HAL (not register-based) */
typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data,
                  uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} sgp40_i2c_t;

typedef struct {
    uint16_t sraw_voc;
} sgp40_data_t;

typedef struct {
    const sgp40_i2c_t *i2c;
} sgp40_t;

#define SGP40_DEV_ADDR  0x59u

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t sgp40_crc8(const uint8_t *data, uint16_t len);

/** Run self-test (command 0x280E), wait 320ms, check response.
 *  @thread_safe: no @isr_safe: no */
sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c);

/** Measure raw VOC signal with default compensation (50% RH, 25C).
 *  @thread_safe: no @isr_safe: no */
sgp40_err_t sgp40_measure_raw(sgp40_t *dev, sgp40_data_t *out);

/** Measure raw VOC signal with custom humidity/temperature compensation.
 *  humidity: 0x0000..0xFFFF (0..100% RH, 0x8000 = 50%)
 *  temperature: 0x0000..0xFFFF (-45..130C, 0x6666 = 25C)
 *  @thread_safe: no @isr_safe: no */
sgp40_err_t sgp40_measure_raw_comp(sgp40_t *dev, sgp40_data_t *out,
                                   uint16_t humidity, uint16_t temperature);

#ifdef __cplusplus
}
#endif

#endif /* SGP40_H */
