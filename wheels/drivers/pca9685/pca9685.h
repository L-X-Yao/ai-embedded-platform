// SPDX-License-Identifier: MIT
/*
 * pca9685 — I2C 16-channel 12-bit PWM driver (NXP PCA9685)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Controls 16 independent PWM channels via I2C. Supports raw on/off
 * counts, duty-cycle percentage, and servo pulse widths. Configurable
 * PWM frequency 24-1526 Hz, sleep/wake power management.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PCA9685_OK         =  0,
    PCA9685_ERR_I2C    = -1,
    PCA9685_ERR_PARAM = -2,
} pca9685_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} pca9685_i2c_t;

typedef struct {
    const pca9685_i2c_t *i2c;
    uint8_t              dev_addr;   /* 7-bit I2C address (typically 0x40) */
    uint16_t             freq_hz;    /* current PWM frequency */
} pca9685_t;

#define PCA9685_ADDR_DEFAULT  0x40u
#define PCA9685_CHANNELS      16u
#define PCA9685_COUNT_MAX     4095u
#define PCA9685_FREQ_MIN      24u
#define PCA9685_FREQ_MAX      1526u

/** Initialize and configure PCA9685. @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_init(pca9685_t *dev, const pca9685_i2c_t *i2c,
                           uint8_t dev_addr, uint16_t freq_hz);

/** Set raw PWM on/off counts (0-4095) for a channel (0-15). @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel,
                              uint16_t on, uint16_t off);

/** Set duty cycle (0.0-100.0%) for a channel. @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_set_duty(pca9685_t *dev, uint8_t channel, float duty_pct);

/** Set servo pulse width in microseconds for a channel. @thread_safe: no @isr_safe: yes */
pca9685_err_t pca9685_set_servo_us(pca9685_t *dev, uint8_t channel,
                                   uint16_t pulse_us);

/** Turn all channels off. @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_all_off(pca9685_t *dev);

/** Set PWM frequency (24-1526 Hz). Briefly enters sleep mode. @thread_safe: no @isr_safe: yes */
pca9685_err_t pca9685_set_freq(pca9685_t *dev, uint16_t freq_hz);

/** Enter low-power sleep mode (outputs off). @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_sleep(pca9685_t *dev);

/** Wake from sleep mode. @thread_safe: no @isr_safe: no */
pca9685_err_t pca9685_wake(pca9685_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* PCA9685_H */
