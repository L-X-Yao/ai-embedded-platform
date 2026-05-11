// SPDX-License-Identifier: MIT
/*
 * tb6612 — Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal + pwm_hal (injected)
 * Instances: multi
 *
 * Two motor channels (A and B), each with IN1/IN2 direction pins and
 * a PWM speed pin.  STBY pin controls standby mode.
 * Truth table: IN1=H,IN2=L -> CW; IN1=L,IN2=H -> CCW;
 *              IN1=L,IN2=L -> stop; IN1=H,IN2=H -> brake.
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
} tb6612_channel_t;

/* ---------- pin identifiers in config ---------- */

typedef struct {
    uint8_t ain1;
    uint8_t ain2;
    uint8_t bin1;
    uint8_t bin2;
    uint8_t stby;
    uint8_t pwma;
    uint8_t pwmb;
} tb6612_pins_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*write_pin)(void *ctx, uint8_t pin_id, bool level);
    void (*set_pwm_duty)(void *ctx, uint8_t channel, uint16_t duty);
    void *ctx;
} tb6612_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const tb6612_hal_t *hal;
    tb6612_pins_t       pins;
    uint16_t            max_duty;
} tb6612_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Initialize driver, pull STBY low (disabled). */
tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_hal_t *hal,
                         const tb6612_pins_t *pins, uint16_t max_duty);

/**
 * Set motor speed.  speed_permille: -1000..+1000.
 * Positive = CW (IN1=H, IN2=L), negative = CCW (IN1=L, IN2=H).
 * Zero = stop (IN1=L, IN2=L, PWM=0).
 */
tb6612_err_t tb6612_set_speed(tb6612_t *dev, tb6612_channel_t channel,
                              int16_t speed_permille);

/** Brake motor (IN1=H, IN2=H). */
tb6612_err_t tb6612_brake(tb6612_t *dev, tb6612_channel_t channel);

/** Set standby mode. on=true -> STBY LOW (disabled), on=false -> STBY HIGH (active). */
tb6612_err_t tb6612_standby(tb6612_t *dev, bool on);

#ifdef __cplusplus
}
#endif

#endif /* TB6612_H */
