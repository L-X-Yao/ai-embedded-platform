// SPDX-License-Identifier: MIT
/*
 * bq27441 — TI BQ27441-G1 I2C smart fuel gauge driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Supports reading voltage, current, SOC, temperature, capacity,
 * and power. Configuration mode for setting design capacity.
 * Seal/unseal for security. All registers are 16-bit little-endian.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BQ27441_H
#define BQ27441_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BQ27441_OK          =  0,
    BQ27441_ERR_I2C     = -1,
    BQ27441_ERR_ID      = -2,
    BQ27441_ERR_PARAM   = -3,
    BQ27441_ERR_TIMEOUT = -4,
    BQ27441_ERR_SEALED  = -5,
    BQ27441_ERR_NO_BATTERY = -6,
    BQ27441_ERR_UNSUPPORTED = -7,
} bq27441_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required */
    void *ctx;
} bq27441_i2c_t;

/* ---------- constants ---------- */

#define BQ27441_DEV_ADDR     0x55u
#define BQ27441_DEVICE_TYPE  0x0421u

/* ---------- data types ---------- */

typedef struct {
    uint16_t voltage_mv;        /* battery voltage in mV */
    int16_t  current_ma;        /* average current in mA (negative = discharge) */
    uint16_t soc_pct;           /* state of charge 0-100% */
    int16_t  temperature_01c;   /* temperature in 0.1 degC */
    uint16_t remaining_mah;     /* remaining capacity in mAh */
    uint16_t full_charge_mah;   /* full charge capacity in mAh */
    int16_t  avg_power_mw;      /* average power in mW (negative = discharge) */
} bq27441_data_t;

/* ---------- device context ---------- */

typedef struct {
    const bq27441_i2c_t *i2c;
} bq27441_t;

/* ---------- API ---------- */

/** Initialize and verify device ID (0x0421). @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_init(bq27441_t *dev, const bq27441_i2c_t *i2c);

/** Read all fuel gauge data into out. Returns BQ27441_ERR_NO_BATTERY when the
 *  gauge detects no battery, as its predictions are then not valid.
 *  @thread_safe: caller_must_lock @isr_safe: no */
bq27441_err_t bq27441_read(bq27441_t *dev, bq27441_data_t *out);

/** Read battery voltage in mV. @thread_safe: caller_must_lock @isr_safe: no */
bq27441_err_t bq27441_read_voltage(bq27441_t *dev, uint16_t *voltage_mv);

/** Read state of charge (0-100%). @thread_safe: caller_must_lock @isr_safe: no */
bq27441_err_t bq27441_read_soc(bq27441_t *dev, uint16_t *soc_pct);

/** Read average current in mA (negative = discharge). @thread_safe: caller_must_lock @isr_safe: no */
bq27441_err_t bq27441_read_current(bq27441_t *dev, int16_t *current_ma);

/** Read temperature in 0.1 degC. @thread_safe: caller_must_lock @isr_safe: no */
bq27441_err_t bq27441_read_temperature(bq27441_t *dev, int16_t *temperature_01c);

/** Enter config update mode (unseal first if sealed). @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_enter_config_mode(bq27441_t *dev);

/** Exit config update mode and reseal. @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_exit_config_mode(bq27441_t *dev);

/** Not available yet: returns BQ27441_ERR_UNSUPPORTED, as the value needs block
 *  data access. @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_set_design_capacity(bq27441_t *dev, uint16_t mah);

/** Send unseal keys. Returns BQ27441_ERR_SEALED if the gauge stayed sealed.
 *  @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_unseal(bq27441_t *dev);

/** Seal the device (restrict access to control/config). @thread_safe: no @isr_safe: no */
bq27441_err_t bq27441_seal(bq27441_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BQ27441_H */
