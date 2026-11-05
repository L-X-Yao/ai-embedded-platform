// SPDX-License-Identifier: MIT
/*
 * htu21d — TE HTU21D digital humidity and temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command-based I2C protocol (no register addresses).
 * CRC-8 protected 3-byte responses.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef HTU21D_H
#define HTU21D_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HTU21D_OK        =  0,
    HTU21D_ERR_I2C   = -1,
    HTU21D_ERR_PARAM = -2,
    HTU21D_ERR_CRC   = -3,
} htu21d_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_cmd)(void *ctx, uint8_t dev_addr, uint8_t cmd);
    bool (*read_data)(void *ctx, uint8_t dev_addr, uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} htu21d_i2c_t;

/* ---------- constants ---------- */

#define HTU21D_DEV_ADDR          0x40u

#define HTU21D_CMD_TRIG_TEMP     0xE3u  /* trigger temperature, hold master */
#define HTU21D_CMD_TRIG_HUM      0xE5u  /* trigger humidity, hold master    */
#define HTU21D_CMD_SOFT_RESET    0xFEu  /* soft reset                       */

/* ---------- device context ---------- */

typedef struct {
    const htu21d_i2c_t *i2c;
} htu21d_t;

/* ---------- API ---------- */

/** Initialise device and issue soft reset. Waits 15 ms first, because the
 *  sensor takes up to that long after power up to accept a command, and 15 ms
 *  after the reset. With delay_ms NULL the caller must make both waits.
 *  @thread_safe: no @isr_safe: no */
htu21d_err_t htu21d_init(htu21d_t *dev, const htu21d_i2c_t *i2c);

/** Read temperature in degrees Celsius. @thread_safe: caller_must_lock @isr_safe: no */
htu21d_err_t htu21d_read_temperature(htu21d_t *dev, float *temp_c);

/** Read relative humidity in percent. @thread_safe: caller_must_lock @isr_safe: no */
htu21d_err_t htu21d_read_humidity(htu21d_t *dev, float *rh_percent);

/** Issue soft reset. @thread_safe: no @isr_safe: no */
htu21d_err_t htu21d_reset(htu21d_t *dev);

/** CRC-8 used by HTU21D (poly 0x131, init 0x00). */
uint8_t htu21d_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* HTU21D_H */
