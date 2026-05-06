// SPDX-License-Identifier: MIT
/*
 * sgp30 — Sensirion SGP30 air quality sensor driver (TVOC + eCO2)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 *
 * SGP30 uses 16-bit I2C commands (MSB first). Responses are CRC-8
 * protected (poly 0x31, init 0xFF — same as SHT series).
 * Measurement returns TVOC (0..60000 ppb) and eCO2 (400..60000 ppm).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SGP30_H
#define SGP30_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SGP30_OK        =  0,
    SGP30_ERR_I2C   = -1,
    SGP30_ERR_CRC   = -2,
    SGP30_ERR_PARAM = -3,
} sgp30_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint16_t ms);
    void *ctx;
} sgp30_i2c_t;

/* ---------- constants ---------- */

#define SGP30_DEV_ADDR  0x58u

/* ---------- data types ---------- */

typedef struct {
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
} sgp30_data_t;

typedef struct {
    uint16_t tvoc_baseline;
    uint16_t eco2_baseline;
} sgp30_baseline_t;

typedef struct {
    uint8_t id[6];   /* 48-bit serial, 3 words x 2 bytes */
} sgp30_serial_t;

/* ---------- device context ---------- */

typedef struct {
    const sgp30_i2c_t *i2c;
} sgp30_t;

/* ---------- API ---------- */

/** Send iaq_init command (0x2003) to start air quality measurement.
 *  @thread_safe: no @isr_safe: no */
sgp30_err_t sgp30_init(sgp30_t *dev, const sgp30_i2c_t *i2c);

/** Measure IAQ: send 0x2008, wait 12ms, read TVOC + eCO2.
 *  Must be called once per second after init for algorithm to converge.
 *  @thread_safe: caller_must_lock @isr_safe: no */
sgp30_err_t sgp30_measure_iaq(sgp30_t *dev, sgp30_data_t *out);

/** Read baseline values (0x2015) for persistence across power cycles.
 *  @thread_safe: no @isr_safe: no */
sgp30_err_t sgp30_get_baseline(sgp30_t *dev, sgp30_baseline_t *out);

/** Restore previously saved baseline values (0x201E).
 *  @thread_safe: no @isr_safe: no */
sgp30_err_t sgp30_set_baseline(sgp30_t *dev, const sgp30_baseline_t *bl);

/** Read 48-bit serial number (0x3682).
 *  @thread_safe: no @isr_safe: no */
sgp30_err_t sgp30_get_serial_id(sgp30_t *dev, sgp30_serial_t *out);

/** Set absolute humidity for compensation (0x2061).
 *  Value is in g/m3, fixed-point 8.8 format (e.g., 15.5 g/m3 = 0x0F80).
 *  Pass 0x0000 to disable compensation.
 *  @thread_safe: no @isr_safe: no */
sgp30_err_t sgp30_set_humidity(sgp30_t *dev, uint16_t abs_humidity_8_8);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t sgp30_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SGP30_H */
