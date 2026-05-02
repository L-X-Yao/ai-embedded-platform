// SPDX-License-Identifier: MIT
/*
 * stepper — step/dir motor driver with trapezoidal acceleration
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Drives step/dir stepper drivers (A4988, DRV8825, TMC2209, etc.).
 * Trapezoidal acceleration profile with integer math, no float.
 * Blocking move via caller-provided delay_us callback.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the stepper_t struct.
 */

#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STEPPER_OK         =  0,
    STEPPER_ERR_PARAM = -1,
    STEPPER_ERR_BUSY   = -2,
} stepper_err_t;

typedef enum {
    STEPPER_DIR_CW  = 0,
    STEPPER_DIR_CCW = 1,
} stepper_dir_t;

typedef enum {
    STEPPER_STATE_IDLE  = 0,
    STEPPER_STATE_ACCEL,
    STEPPER_STATE_CRUISE,
    STEPPER_STATE_DECEL,
} stepper_state_t;

typedef struct {
    void (*step_pin)(void *ctx, bool level);
    void (*dir_pin)(void *ctx, bool level);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} stepper_hal_t;

typedef struct {
    const stepper_hal_t *hal;
    uint16_t max_speed;        /* steps/sec */
    uint16_t accel;            /* steps/sec^2 */
    int32_t  position;         /* current position in steps */
    /* internal ramp state */
    stepper_state_t state;
    uint32_t step_count;
    uint32_t accel_steps;
    uint32_t decel_start;
    uint32_t total_steps;
    uint32_t current_delay_us;
} stepper_t;

/** Initialize stepper driver. @thread_safe: no */
stepper_err_t stepper_init(stepper_t *s, const stepper_hal_t *hal,
                           uint16_t max_speed, uint16_t accel);

/** Move relative (blocking, uses delay_us callback). @thread_safe: no */
stepper_err_t stepper_move(stepper_t *s, int32_t steps);

/** Move to absolute position (blocking). @thread_safe: no */
stepper_err_t stepper_move_to(stepper_t *s, int32_t target);

/** Single step for manual control. @thread_safe: no */
void stepper_step_once(stepper_t *s, stepper_dir_t dir);

/** Get current position. @thread_safe: no */
int32_t stepper_get_position(const stepper_t *s);

/** Set position without moving (home/calibrate). @thread_safe: no */
void stepper_set_position(stepper_t *s, int32_t pos);

/** Get current state. @thread_safe: no */
stepper_state_t stepper_get_state(const stepper_t *s);

#ifdef __cplusplus
}
#endif

#endif /* STEPPER_H */
