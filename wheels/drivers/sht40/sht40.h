// SPDX-License-Identifier: MIT
/*
 * sht40 — Sensirion SHT40 temperature and humidity sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * SHT40 uses a command-response I2C protocol: send 1-byte command,
 * wait for measurement, read 6 bytes (2 temp + CRC + 2 hum + CRC).
 * CRC-8 polynomial 0x31, init 0xFF (Sensirion standard).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SHT40_H
#define SHT40_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SHT40_OK        =  0,
    SHT40_ERR_I2C   = -1,
    SHT40_ERR_CRC   = -2,
    SHT40_ERR_PARAM = -3,
} sht40_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint16_t ms);
    void *ctx;
} sht40_i2c_t;

/* ---------- constants ---------- */

#define SHT40_ADDR_A  0x44
#define SHT40_ADDR_B  0x45

/* ---------- data types ---------- */

typedef struct {
    float temperature_c;
    float humidity_pct;
} sht40_data_t;

/* ---------- device context ---------- */

typedef struct {
    const sht40_i2c_t *i2c;
    uint8_t addr;
    uint32_t serial;
} sht40_t;

/* ---------- API ---------- */

/** Soft reset + read serial number. @thread_safe: no @isr_safe: no */
sht40_err_t sht40_init(sht40_t *dev, const sht40_i2c_t *i2c, uint8_t addr);

/** High-precision measurement (cmd 0xFD, ~10ms). @thread_safe: caller_must_lock @isr_safe: no */
sht40_err_t sht40_read_high(sht40_t *dev, sht40_data_t *out);

/** Medium-precision measurement (cmd 0xF6, ~5ms). @thread_safe: caller_must_lock @isr_safe: no */
sht40_err_t sht40_read_medium(sht40_t *dev, sht40_data_t *out);

/** Low-precision measurement (cmd 0xE0, ~2ms). @thread_safe: caller_must_lock @isr_safe: no */
sht40_err_t sht40_read_low(sht40_t *dev, sht40_data_t *out);

/** Soft reset (cmd 0x94). @thread_safe: no @isr_safe: no */
sht40_err_t sht40_soft_reset(sht40_t *dev);

/** Read serial number (cmd 0x89). @thread_safe: no @isr_safe: no */
sht40_err_t sht40_read_serial(sht40_t *dev, uint32_t *serial);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t sht40_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SHT40_H */
