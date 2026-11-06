// SPDX-License-Identifier: MIT
/*
 * ads1015 — TI ADS1015 12-bit 4-channel ADC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C register-based interface. Supports single-shot mode, configurable
 * gain (PGA), data rate, and mux channel selection. 12-bit result is
 * left-justified in a 16-bit register (shift right 4 to get value).
 * Pure C99, no heap. Thread safety: caller owns the ads1015_t struct.
 */

#ifndef ADS1015_H
#define ADS1015_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADS1015_OK          =  0,
    ADS1015_ERR_I2C     = -1,
    ADS1015_ERR_PARAM   = -2,
    ADS1015_ERR_BUSY    = -3,
} ads1015_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ads1015_i2c_t;

typedef enum {
    ADS1015_MUX_AIN0_AIN1 = 0,
    ADS1015_MUX_AIN0_AIN3 = 1,
    ADS1015_MUX_AIN1_AIN3 = 2,
    ADS1015_MUX_AIN2_AIN3 = 3,
    ADS1015_MUX_AIN0_GND  = 4,
    ADS1015_MUX_AIN1_GND  = 5,
    ADS1015_MUX_AIN2_GND  = 6,
    ADS1015_MUX_AIN3_GND  = 7,
} ads1015_mux_t;

typedef enum {
    ADS1015_PGA_6144 = 0,
    ADS1015_PGA_4096 = 1,
    ADS1015_PGA_2048 = 2,  /* default */
    ADS1015_PGA_1024 = 3,
    ADS1015_PGA_0512 = 4,
    ADS1015_PGA_0256 = 5,
} ads1015_pga_t;

typedef enum {
    ADS1015_RATE_128  = 0,
    ADS1015_RATE_250  = 1,
    ADS1015_RATE_490  = 2,
    ADS1015_RATE_920  = 3,
    ADS1015_RATE_1600 = 4,  /* default */
    ADS1015_RATE_2400 = 5,
    ADS1015_RATE_3300 = 6,
} ads1015_rate_t;

typedef struct {
    uint8_t        addr;
    ads1015_pga_t  pga;
    ads1015_rate_t rate;
} ads1015_cfg_t;

typedef struct {
    const ads1015_i2c_t  *i2c;
    ads1015_cfg_t  cfg;
} ads1015_t;

#define ADS1015_ADDR_GND  0x48u
#define ADS1015_ADDR_VDD  0x49u
#define ADS1015_ADDR_SDA  0x4Au
#define ADS1015_ADDR_SCL  0x4Bu

/** Initialise device handle. @thread_safe: no @isr_safe: no */
ads1015_err_t ads1015_init(ads1015_t *dev, const ads1015_i2c_t *i2c,
                           const ads1015_cfg_t *cfg);

/** Single-shot read on a channel, result in millivolts. @thread_safe: caller_must_lock @isr_safe: no */
ads1015_err_t ads1015_read_single(ads1015_t *dev, ads1015_mux_t channel,
                                  float *voltage_mv);

/** Read raw 12-bit ADC value (sign-extended). @thread_safe: caller_must_lock @isr_safe: no */
ads1015_err_t ads1015_read_raw(ads1015_t *dev, ads1015_mux_t channel,
                               int16_t *raw);

/** Change PGA gain for subsequent reads. @thread_safe: caller_must_lock @isr_safe: yes */
ads1015_err_t ads1015_set_pga(ads1015_t *dev, ads1015_pga_t pga);

/** Change data rate for subsequent reads. @thread_safe: caller_must_lock @isr_safe: yes */
ads1015_err_t ads1015_set_rate(ads1015_t *dev, ads1015_rate_t rate);

#ifdef __cplusplus
}
#endif

#endif /* ADS1015_H */
