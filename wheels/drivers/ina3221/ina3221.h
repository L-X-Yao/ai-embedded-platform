// SPDX-License-Identifier: MIT
/*
 * ina3221 — TI INA3221 triple-channel current and power monitor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef INA3221_H
#define INA3221_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    INA3221_OK        =  0,
    INA3221_ERR_I2C   = -1,
    INA3221_ERR_ID    = -2,
    INA3221_ERR_PARAM = -3,
} ina3221_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ina3221_i2c_t;

/* ---------- constants ---------- */

#define INA3221_ADDR_GND  0x40u
#define INA3221_ADDR_VS   0x41u
#define INA3221_ADDR_SDA  0x42u
#define INA3221_ADDR_SCL  0x43u

#define INA3221_NUM_CHANNELS  3

/* ---------- configuration ---------- */

typedef struct {
    uint8_t addr;                                   /* 7-bit I2C address */
    float   shunt_resistor_ohm[INA3221_NUM_CHANNELS]; /* shunt per channel */
} ina3221_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float shunt_uv[INA3221_NUM_CHANNELS];   /* shunt voltage in microvolts   */
    float bus_mv[INA3221_NUM_CHANNELS];     /* bus voltage in millivolts     */
    float current_ma[INA3221_NUM_CHANNELS]; /* current in milliamps          */
} ina3221_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ina3221_i2c_t *i2c;
    ina3221_cfg_t        cfg;
} ina3221_t;

/* ---------- API ---------- */

/** Initialise and verify manufacturer/die ID.
 *  @thread_safe: no @isr_safe: no */
ina3221_err_t ina3221_init(ina3221_t *dev,
                           const ina3221_i2c_t *i2c,
                           const ina3221_cfg_t *cfg);

/** Read one channel (0-2): shunt voltage (uV) and bus voltage (mV).
 *  @thread_safe: caller_must_lock @isr_safe: no */
ina3221_err_t ina3221_read_channel(const ina3221_t *dev,
                                   uint8_t channel,
                                   float *shunt_uv,
                                   float *bus_mv);

/** Read all 3 channels into ina3221_data_t (includes current calc).
 *  @thread_safe: caller_must_lock @isr_safe: no */
ina3221_err_t ina3221_read_all(const ina3221_t *dev,
                               ina3221_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* INA3221_H */
