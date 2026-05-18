// SPDX-License-Identifier: MIT
/*
 * ads1115 — TI ADS1115 16-bit 4-channel ADC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C register-based interface. Supports single-shot mode, configurable
 * gain (PGA), and mux channel selection. Pure C99, no heap.
 * Thread safety: caller owns the ads1115_t struct.
 */

#ifndef ADS1115_H
#define ADS1115_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADS1115_OK          =  0,
    ADS1115_ERR_I2C     = -1,
    ADS1115_ERR_PARAM   = -2,
    ADS1115_ERR_BUSY    = -3,
} ads1115_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ads1115_i2c_t;

typedef enum {
    ADS1115_MUX_AIN0_AIN1 = 0,
    ADS1115_MUX_AIN0_AIN3 = 1,
    ADS1115_MUX_AIN1_AIN3 = 2,
    ADS1115_MUX_AIN2_AIN3 = 3,
    ADS1115_MUX_AIN0_GND  = 4,
    ADS1115_MUX_AIN1_GND  = 5,
    ADS1115_MUX_AIN2_GND  = 6,
    ADS1115_MUX_AIN3_GND  = 7,
} ads1115_mux_t;

typedef enum {
    ADS1115_PGA_6144 = 0,
    ADS1115_PGA_4096 = 1,
    ADS1115_PGA_2048 = 2,
    ADS1115_PGA_1024 = 3,
    ADS1115_PGA_0512 = 4,
    ADS1115_PGA_0256 = 5,
} ads1115_pga_t;

typedef enum {
    ADS1115_RATE_8   = 0,
    ADS1115_RATE_16  = 1,
    ADS1115_RATE_32  = 2,
    ADS1115_RATE_64  = 3,
    ADS1115_RATE_128 = 4,
    ADS1115_RATE_250 = 5,
    ADS1115_RATE_475 = 6,
    ADS1115_RATE_860 = 7,
} ads1115_rate_t;

typedef struct {
    uint8_t       addr;
    ads1115_mux_t mux;
    ads1115_pga_t pga;
    ads1115_rate_t rate;
} ads1115_cfg_t;

typedef struct {
    const ads1115_i2c_t *i2c;
    ads1115_cfg_t cfg;
} ads1115_t;

#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49
#define ADS1115_ADDR_SDA  0x4A
#define ADS1115_ADDR_SCL  0x4B

/** @thread_safe: no @isr_safe: no */
ads1115_err_t ads1115_init(ads1115_t *dev, const ads1115_i2c_t *i2c,
                           const ads1115_cfg_t *cfg);

/** Read raw 16-bit ADC value from configured channel. @thread_safe: caller_must_lock @isr_safe: no */
ads1115_err_t ads1115_read_raw(ads1115_t *dev, int16_t *raw);

/** Read ADC value converted to millivolts using PGA setting. @thread_safe: caller_must_lock @isr_safe: yes */
ads1115_err_t ads1115_read_voltage(ads1115_t *dev, float *voltage_mv);

/** Set input mux channel for next conversion. @thread_safe: caller_must_lock @isr_safe: yes */
ads1115_err_t ads1115_set_channel(ads1115_t *dev, ads1115_mux_t mux);

#ifdef __cplusplus
}
#endif

#endif /* ADS1115_H */
