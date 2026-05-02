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
 * @thread_safe: no
 */
pid_controller_err_t pid_controller_init(pid_controller_t *ctx,
                                         const pid_controller_cfg_t *cfg);

/**
 * @thread_safe: no
 */
pid_controller_err_t pid_controller_reset(pid_controller_t *ctx);

/**
 * Compute one PID iteration.
 *
 * @param ctx       Controller context.
 * @param setpoint  Desired value.
 * @param measured  Current measurement.
 * @param dt        Time step in seconds (must be > 0).
 * @return          Clamped output value, or 0.0f on error.
 * @thread_safe: no
 */
float pid_controller_update(pid_controller_t *ctx, float setpoint,
                            float measured, float dt);

/**
 * @thread_safe: no
 */
pid_controller_err_t pid_controller_set_tunings(pid_controller_t *ctx,
                                                float kp, float ki, float kd);

/**
 * @thread_safe: no
 */
pid_controller_err_t pid_controller_set_output_limits(pid_controller_t *ctx,
                                                      float min, float max);

#ifdef __cplusplus
}
#endif

#endif /* PID_CONTROLLER_H */
