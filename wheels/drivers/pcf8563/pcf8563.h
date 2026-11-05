// SPDX-License-Identifier: MIT
/*
 * pcf8563 — NXP PCF8563 I2C real-time clock driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * BCD-encoded time/date registers. 24-hour mode.
 * Alarm, timer, and CLKOUT support.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef PCF8563_H
#define PCF8563_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    PCF8563_OK        =  0,
    PCF8563_ERR_I2C   = -1,
    PCF8563_ERR_ID    = -2,
    PCF8563_ERR_PARAM = -3,
} pcf8563_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} pcf8563_i2c_t;

/* ---------- constants ---------- */

#define PCF8563_DEV_ADDR  0x51

/* ---------- CLKOUT frequencies ---------- */

typedef enum {
    PCF8563_CLKOUT_32768HZ = 0,
    PCF8563_CLKOUT_1024HZ  = 1,
    PCF8563_CLKOUT_32HZ    = 2,
    PCF8563_CLKOUT_1HZ     = 3,
} pcf8563_clkout_t;

/* ---------- timer clock source ---------- */

typedef enum {
    PCF8563_TIMER_4096HZ = 0,   /* 244 us resolution */
    PCF8563_TIMER_64HZ   = 1,   /* 15.625 ms resolution */
    PCF8563_TIMER_1HZ    = 2,   /* 1 s resolution */
    PCF8563_TIMER_1_60HZ = 3,   /* 1 min resolution */
} pcf8563_timer_clk_t;

/* ---------- data types ---------- */

typedef struct {
    uint8_t year;     /* 0-99 (2000-2099) */
    uint8_t month;    /* 1-12 */
    uint8_t day;      /* 1-31 */
    uint8_t weekday;  /* 0-6 (0=Sunday) */
    uint8_t hour;     /* 0-23 */
    uint8_t minute;   /* 0-59 */
    uint8_t second;   /* 0-59 */
} pcf8563_time_t;

typedef struct {
    uint8_t minute;   /* 0-59, or 0x80 = disabled */
    uint8_t hour;     /* 0-23, or 0x80 = disabled */
    uint8_t day;      /* 1-31, or 0x80 = disabled */
    uint8_t weekday;  /* 0-6,  or 0x80 = disabled */
} pcf8563_alarm_t;

/* ---------- device context ---------- */

typedef struct {
    const pcf8563_i2c_t *i2c;
} pcf8563_t;

/* ---------- API ---------- */

/** Initialize and clear control registers. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_init(pcf8563_t *dev,
                               const pcf8563_i2c_t *i2c);

/** Read current time from RTC. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_read_time(const pcf8563_t *dev,
                                    pcf8563_time_t *time);

/** Write time to RTC. The HAL has no multi-byte write, so this holds STOP
 *  across the seven register writes, then clears it, keeping TEST1 and TESTC.
 *  The clock loses the time between STOP and release, and its first increment
 *  comes 0.507813 s to 0.507935 s after release. CLKOUT at 1.024 kHz, 32 Hz or
 *  1 Hz stops meanwhile. A failed write still attempts the release, and the
 *  time may then be part written. If the release itself fails the clock stays
 *  stopped until a later write_time or init.
 *  @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_write_time(const pcf8563_t *dev,
                                     const pcf8563_time_t *time);

/** Read alarm settings. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_read_alarm(const pcf8563_t *dev,
                                     pcf8563_alarm_t *alarm);

/** Write alarm settings. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_write_alarm(const pcf8563_t *dev,
                                      const pcf8563_alarm_t *alarm);

/** Enable countdown timer. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_enable_timer(const pcf8563_t *dev,
                                       pcf8563_timer_clk_t clk,
                                       uint8_t countdown);

/** Disable countdown timer. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_disable_timer(const pcf8563_t *dev);

/** Configure CLKOUT pin frequency, or disable it. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_set_clkout(const pcf8563_t *dev,
                                     bool enable,
                                     pcf8563_clkout_t freq);

#ifdef __cplusplus
}
#endif

#endif /* PCF8563_H */
