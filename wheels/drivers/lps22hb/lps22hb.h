// SPDX-License-Identifier: MIT
/*
 * lps22hb — ST LPS22HB barometric pressure sensor driver (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 24-bit pressure, 16-bit temperature. Supports one-shot and continuous
 * ODR modes (1/10/25/50/75 Hz). Conversion per LPS22HB datasheet:
 *   pressure = raw / 4096.0 (hPa)
 *   temperature = raw / 100.0 (deg C)
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef LPS22HB_H
#define LPS22HB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LPS22HB_OK        =  0,
    LPS22HB_ERR_I2C   = -1,
    LPS22HB_ERR_ID    = -2,
    LPS22HB_ERR_PARAM = -3,
} lps22hb_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} lps22hb_i2c_t;

/* ---------- constants ---------- */

#define LPS22HB_DEV_ADDR     0x5C
#define LPS22HB_DEV_ADDR_ALT 0x5D
#define LPS22HB_CHIP_ID      0xB1

/* ---------- ODR enum ---------- */

typedef enum {
    LPS22HB_ODR_ONE_SHOT = 0,   /* power-down / one-shot mode        */
    LPS22HB_ODR_1HZ      = 1,   /* 1 Hz continuous                   */
    LPS22HB_ODR_10HZ     = 2,   /* 10 Hz continuous                  */
    LPS22HB_ODR_25HZ     = 3,   /* 25 Hz continuous                  */
    LPS22HB_ODR_50HZ     = 4,   /* 50 Hz continuous                  */
    LPS22HB_ODR_75HZ     = 5,   /* 75 Hz continuous                  */
} lps22hb_odr_t;

/* ---------- data types ---------- */

typedef struct {
    float pressure_hpa;
    float temperature_c;
} lps22hb_data_t;

/* ---------- device context ---------- */

typedef struct {
    const lps22hb_i2c_t *i2c;
    uint8_t addr;
} lps22hb_t;

/* ---------- API ---------- */

/** Configure and verify chip ID (WHO_AM_I = 0xB1).
 *  Sets ODR to one-shot (power-down) mode.
 *  @thread_safe: no @isr_safe: no */
lps22hb_err_t lps22hb_init(lps22hb_t *dev, const lps22hb_i2c_t *i2c);

/** Read pressure (hPa) and temperature (deg C).
 *  In one-shot mode, caller must trigger first via lps22hb_one_shot_trigger().
 *  In continuous mode, reads latest sample.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lps22hb_err_t lps22hb_read(lps22hb_t *dev, lps22hb_data_t *out);

/** Set output data rate. LPS22HB_ODR_ONE_SHOT enters power-down.
 *  @thread_safe: no @isr_safe: no */
lps22hb_err_t lps22hb_set_odr(lps22hb_t *dev, lps22hb_odr_t odr);

/** Trigger a one-shot measurement (only valid when ODR = ONE_SHOT).
 *  @thread_safe: no @isr_safe: no */
lps22hb_err_t lps22hb_one_shot_trigger(lps22hb_t *dev);

/** Read raw 24-bit pressure register value.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lps22hb_err_t lps22hb_read_raw_pressure(lps22hb_t *dev, int32_t *raw);

/** Read raw 16-bit temperature register value.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lps22hb_err_t lps22hb_read_raw_temperature(lps22hb_t *dev, int16_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* LPS22HB_H */
