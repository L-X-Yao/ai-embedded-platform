// SPDX-License-Identifier: MIT
/*
 * drv8833 — dual H-bridge DC motor driver (DRV8833/DRV8835)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 *
 * Controls two DC motors via 4 PWM pins (AIN1, AIN2, BIN1, BIN2).
 * Forward/reverse/brake/coast per channel.
 * Caller provides PWM duty-set and optional nSLEEP callbacks.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the drv8833_t struct.
 */

#ifndef DRV8833_H
#define DRV8833_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRV8833_OK         =  0,
    DRV8833_ERR_PARAM  = -1,
} drv8833_err_t;

typedef enum {
    DRV8833_CH_A = 0,
    DRV8833_CH_B = 1,
} drv8833_channel_t;

/**
 * HAL callbacks.
 *
 * set_pwm(ctx, channel, pin, duty):
 *   channel = 0 (A) or 1 (B)
 *   pin     = 0 (IN1) or 1 (IN2)
 *   duty    = 0..max_duty
 *
 * set_sleep(ctx, sleep):  optional (may be NULL)
 *   true = enter sleep, false = wake
 */
typedef struct {
    void (*set_pwm)(void *ctx, uint8_t channel, uint8_t pin, uint16_t duty);
    void (*set_sleep)(void *ctx, bool sleep);
    void *ctx;
} drv8833_hal_t;

typedef struct {
    const drv8833_hal_t *hal;
    uint16_t max_duty;     /* PWM resolution (e.g. 1000 for 0.1% steps) */
} drv8833_t;

/** Initialize device. @thread_safe: no */
drv8833_err_t drv8833_init(drv8833_t *dev, const drv8833_hal_t *hal,
                           uint16_t max_duty);

/**
 * Set motor speed and direction.
 * speed: -max_duty..+max_duty. Positive = forward, negative = reverse.
 * Clamped to [-max_duty, +max_duty].
 * @thread_safe: no
 */
drv8833_err_t drv8833_set_speed(drv8833_t *dev, drv8833_channel_t ch,
                                int16_t speed);

/** Brake: both pins HIGH (slow decay). @thread_safe: no */
drv8833_err_t drv8833_brake(drv8833_t *dev, drv8833_channel_t ch);

/** Coast: both pins LOW (fast decay). @thread_safe: no */
drv8833_err_t drv8833_coast(drv8833_t *dev, drv8833_channel_t ch);

/** Enter sleep mode (pull nSLEEP low). No-op if set_sleep is NULL. @thread_safe: no */
void drv8833_sleep(drv8833_t *dev);

/** Wake from sleep (pull nSLEEP high). No-op if set_sleep is NULL. @thread_safe: no */
void drv8833_wake(drv8833_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRV8833_H */
