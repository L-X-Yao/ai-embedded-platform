// SPDX-License-Identifier: MIT
/*
 * ds3231 — DS3231 high-precision I2C RTC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 24-hour mode only. BCD encoding handled internally.
 * Temperature readout at 0.25C resolution.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DS3231_OK         =  0,
    DS3231_ERR_I2C    = -1,
    DS3231_ERR_PARAM = -2,
} ds3231_err_t;

typedef struct {
    uint8_t year;     /* 0-99 (2000-2099) */
    uint8_t month;    /* 1-12 */
    uint8_t day;      /* 1-31 */
    uint8_t weekday;  /* 1-7 (1=Monday) */
    uint8_t hour;     /* 0-23 */
    uint8_t minute;   /* 0-59 */
    uint8_t second;   /* 0-59 */
} ds3231_time_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ds3231_i2c_t;

typedef struct {
    const ds3231_i2c_t *i2c;
} ds3231_t;

#define DS3231_ADDR  0x68u

/** Initialize driver context. @thread_safe: no @isr_safe: no */
ds3231_err_t ds3231_init(ds3231_t *dev, const ds3231_i2c_t *i2c);

/** Read current time from RTC. @thread_safe: no @isr_safe: no */
ds3231_err_t ds3231_get_time(const ds3231_t *dev, ds3231_time_t *time);

/** Write time to RTC. @thread_safe: no @isr_safe: no */
ds3231_err_t ds3231_set_time(const ds3231_t *dev, const ds3231_time_t *time);

/** Read on-chip temperature (0.25C resolution). @thread_safe: no @isr_safe: no */
ds3231_err_t ds3231_get_temperature(const ds3231_t *dev, float *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* DS3231_H */
