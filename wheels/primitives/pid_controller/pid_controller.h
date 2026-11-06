// SPDX-License-Identifier: MIT
/*
 * pid_controller — discrete PID controller with anti-windup
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Standard positional PID with configurable output and integral clamping.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PID_CONTROLLER_OK        =  0,
    PID_CONTROLLER_ERR_PARAM = -1,
} pid_controller_err_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_min;
    float output_max;
    float integral_min;
    float integral_max;
} pid_controller_cfg_t;

typedef struct {
    pid_controller_cfg_t cfg;
    float integral;
    float prev_error;
    bool  first_run;
} pid_controller_t;

/**
 * Gains must be finite and limits must not be NaN. An infinite limit means
 * no limit on that side.
 *
 * @thread_safe: no @isr_safe: no
 */
pid_controller_err_t pid_controller_init(pid_controller_t *ctx,
                                         const pid_controller_cfg_t *cfg);

/**
 * @thread_safe: no @isr_safe: no
 */
pid_controller_err_t pid_controller_reset(pid_controller_t *ctx);

/**
 * Compute one PID iteration.
 *
 * @param ctx       Controller context.
 * @param setpoint  Desired value (must be finite).
 * @param measured  Current measurement (must be finite).
 * @param dt        Time step in seconds (must be finite and > 0).
 * @return          Clamped output value, or 0.0f on error. A result that is
 *                  NaN despite finite inputs gives 0.0f, or the nearest limit
 *                  if 0 is outside the output limits.
 * @thread_safe: no @isr_safe: yes
 */
float pid_controller_update(pid_controller_t *ctx, float setpoint,
                            float measured, float dt);

/**
 * Refuses a gain that is not finite and keeps the old gains.
 *
 * @thread_safe: no @isr_safe: yes
 */
pid_controller_err_t pid_controller_set_tunings(pid_controller_t *ctx,
                                                float kp, float ki, float kd);

/**
 * Refuses NaN and min > max and keeps the old limits.
 *
 * @thread_safe: no @isr_safe: yes
 */
pid_controller_err_t pid_controller_set_output_limits(pid_controller_t *ctx,
                                                      float min, float max);

#ifdef __cplusplus
}
#endif

#endif /* PID_CONTROLLER_H */
