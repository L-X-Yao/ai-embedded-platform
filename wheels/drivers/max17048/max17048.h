// SPDX-License-Identifier: MIT
/*
 * max17048 — MAX17048 1-cell and MAX17049 2-cell LiPo fuel gauge (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * ModelGauge IC: reports cell voltage and state-of-charge (SOC%)
 * without current-sense resistor. I2C 7-bit address 0x36.
 */

#ifndef MAX17048_H
#define MAX17048_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MAX17048_OK        =  0,
    MAX17048_ERR_I2C   = -1,
    MAX17048_ERR_ID    = -2,
    MAX17048_ERR_PARAM = -3,
} max17048_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    /* Unused and may be NULL: the chip ignores 8-bit writes, so every write
     * goes through write_raw. Kept so that existing initialisers still build. */
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
    /* Required. Puts exactly data[0..len-1] on the bus after the address,
     * then STOP. max17048_init returns MAX17048_ERR_PARAM without it. */
    bool (*write_raw)(void *ctx, uint8_t dev_addr, const uint8_t *data,
                      uint16_t len);
} max17048_i2c_t;

/* ---------- constants ---------- */

#define MAX17048_DEV_ADDR  0x36

/* ---------- data types ---------- */

typedef struct {
    float    voltage_v;     /* voltage per cell in volts */
    float    soc_pct;       /* state-of-charge 0.0 – 100.0 % */
    float    charge_rate;   /* charge/discharge rate in %/hr (positive=charging) */
} max17048_data_t;

/* ---------- device context ---------- */

typedef struct {
    const max17048_i2c_t *i2c;
} max17048_t;

/* ---------- API ---------- */

/** Verify chip presence by reading VERSION register. @thread_safe: no @isr_safe: no */
max17048_err_t max17048_init(max17048_t *dev, const max17048_i2c_t *i2c);

/** Read voltage, SOC, and charge rate. @thread_safe: caller_must_lock @isr_safe: no */
max17048_err_t max17048_read(max17048_t *dev, max17048_data_t *out);

/** Read cell voltage only (faster, single register). @thread_safe: caller_must_lock @isr_safe: no */
max17048_err_t max17048_read_voltage(max17048_t *dev, float *voltage_v);

/** Read SOC only (faster, single register). @thread_safe: caller_must_lock @isr_safe: no */
max17048_err_t max17048_read_soc(max17048_t *dev, float *soc_pct);

/** Trigger quick-start (recalibrate SOC from current OCV). @thread_safe: no @isr_safe: no */
max17048_err_t max17048_quickstart(max17048_t *dev);

/** Set alert threshold (1-32%, triggers ALRT pin when SOC drops below). @thread_safe: no @isr_safe: no */
max17048_err_t max17048_set_alert_threshold(max17048_t *dev, uint8_t threshold_pct);

/** Clear alert flag after handling. @thread_safe: no @isr_safe: no */
max17048_err_t max17048_clear_alert(max17048_t *dev);

/** Enter sleep mode (ultra-low power, stops ModelGauge). @thread_safe: no @isr_safe: no */
max17048_err_t max17048_sleep(max17048_t *dev);

/** Wake from sleep mode. @thread_safe: no @isr_safe: no */
max17048_err_t max17048_wake(max17048_t *dev);

/** Read IC version register. @thread_safe: caller_must_lock @isr_safe: no */
max17048_err_t max17048_version(max17048_t *dev, uint16_t *ver);

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_H */
