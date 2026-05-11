// SPDX-License-Identifier: MIT
/*
 * tb6612 — Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal + pwm_hal (injected)
 * Instances: multi
 *
 * Two channels (A, B), each with IN1/IN2 direction GPIOs and PWM speed.
 * STBY pin must be HIGH for operation.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the tb6612_t struct.
 */

#ifndef TB6612_H
#define TB6612_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TB6612_OK        =  0,
    TB6612_ERR_PARAM = -1,
} tb6612_err_t;

/* ---------- motor channel ---------- */

typedef enum {
    TB6612_MOTOR_A = 0,
    TB6612_MOTOR_B = 1,
} tb6612_motor_t;

/* ---------- pin IDs ---------- */

typedef struct {
    uint8_t ain1;
    uint8_t ain2;
    uint8_t bin1;
    uint8_t bin2;
    uint8_t stby;
    uint8_t pwm_ch_a;
    uint8_t pwm_ch_b;
} tb6612_cfg_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*write_pin)(void *ctx, uint8_t pin_id, bool level);
    void (*set_pwm_duty)(void *ctx, uint8_t channel, uint16_t duty_permille);
    void *ctx;
} tb6612_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const tb6612_hal_t *hal;
    tb6612_cfg_t cfg;
    bool standby;
} tb6612_t;

/* ---------- API (all @thread_safe: caller_must_lock) @isr_safe: no ---------- */

/** Initialize driver. Puts device in standby (STBY=LOW). */
tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_hal_t *hal,
                         const tb6612_cfg_t *cfg);

/**
 * Set motor speed.
 * speed: -1000 to +1000 (permille). Positive = forward, negative = reverse.
 * Clamped to [-1000, +1000].
 */
tb6612_err_t tb6612_set_speed(tb6612_t *dev, tb6612_motor_t motor,
                              int16_t speed_permille);

/** Brake: both IN pins HIGH, PWM=0. */
tb6612_err_t tb6612_brake(tb6612_t *dev, tb6612_motor_t motor);

/** Enter or exit standby. enable=true → STBY LOW (disabled). */
tb6612_err_t tb6612_standby(tb6612_t *dev, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* TB6612_H */
