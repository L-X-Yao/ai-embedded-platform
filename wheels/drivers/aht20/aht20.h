// SPDX-License-Identifier: MIT
/*
 * aht20 — ASAIR AHT20 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. AHT20 uses a fixed address 0x38,
 * command-based protocol with CRC-8 protected responses (poly 0x31, init 0xFF).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef AHT20_H
#define AHT20_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    AHT20_OK          =  0,
    AHT20_ERR_I2C     = -1,
    AHT20_ERR_CRC     = -2,
    AHT20_ERR_PARAM   = -3,
    AHT20_ERR_NOT_CAL = -4,
} aht20_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint16_t ms);   /* optional, NULL = not needed */
    void *ctx;
} aht20_i2c_t;

/* ---------- constants ---------- */

#define AHT20_ADDR  0x38

/* ---------- data types ---------- */

typedef struct {
    float temperature_c;
    float humidity_pct;
} aht20_data_t;

/* ---------- device context ---------- */

typedef struct {
    const aht20_i2c_t *i2c;
    uint8_t             status;   /* last read status byte */
} aht20_t;

/* ---------- API ---------- */

/** Power-on init: waits 40ms, checks calibration, sends init cmd if needed.
 *  @thread_safe: no @isr_safe: no */
aht20_err_t aht20_init(aht20_t *dev, const aht20_i2c_t *i2c);

/** Trigger measurement, read 7 bytes, CRC-verify, convert.
 *  @thread_safe: no @isr_safe: no */
aht20_err_t aht20_read(aht20_t *dev, aht20_data_t *out);

/** Soft reset (0xBA), waits 20ms.
 *  @thread_safe: no @isr_safe: no */
aht20_err_t aht20_reset(aht20_t *dev);

/** Check if sensor is calibrated (status bit[3]). */
bool aht20_is_calibrated(aht20_t *dev);

/** CRC-8 used by AHT20 (poly 0x31, init 0xFF — same as Sensirion). */
uint8_t aht20_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* AHT20_H */
