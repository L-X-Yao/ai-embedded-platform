// SPDX-License-Identifier: MIT
/*
 * pid_controller — discrete PID controller with anti-windup
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "pid_controller.h"
#include "contract.h"
#include <float.h>

/* False for NaN as well: every comparison with NaN is false. */
static bool is_finite(float x)
{
    return x >= -FLT_MAX && x <= FLT_MAX;
}

/* NaN fails both comparisons and would go straight through to the actuator.
 * It gets what a refused input gets: 0, or the nearest limit if 0 is outside
 * them. Not a limit as such: that would command full scale on a fault. */
static float clampf(float val, float lo, float hi)
{
    if (val != val) val = 0.0f;
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

pid_controller_err_t pid_controller_init(pid_controller_t *ctx,
                                         const pid_controller_cfg_t *cfg)
{
    if (!ctx || !cfg)
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    if (!is_finite(cfg->kp) || !is_finite(cfg->ki) || !is_finite(cfg->kd))
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    /* written so that NaN fails too; infinite limits mean no limit */
    if (!(cfg->output_min <= cfg->output_max) ||
        !(cfg->integral_min <= cfg->integral_max))
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->cfg = *cfg;
    ctx->integral = 0.0f;
    ctx->prev_error = 0.0f;
    ctx->first_run = true;
    ENSURE(ctx->cfg.output_min <= ctx->cfg.output_max);
    ENSURE(ctx->cfg.integral_min <= ctx->cfg.integral_max);
    return PID_CONTROLLER_OK;
}

pid_controller_err_t pid_controller_reset(pid_controller_t *ctx)
{
    if (!ctx)
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->integral = 0.0f;
    ctx->prev_error = 0.0f;
    ctx->first_run = true;
    return PID_CONTROLLER_OK;
}

float pid_controller_update(pid_controller_t *ctx, float setpoint,
                            float measured, float dt)
{
    if (!ctx || dt <= 0.0f)
    {
        return 0.0f;
    }
    /* reject NaN and infinity — 0 * inf is NaN, and NaN stays in the integral */
    if (!is_finite(setpoint) || !is_finite(measured) || !is_finite(dt))
    {
        return 0.0f;
    }

    float error = setpoint - measured;

    /* two finite inputs can still overflow here */
    if (!is_finite(error))
    {
        return 0.0f;
    }

    /* with no integral limit a step can overflow: dropping it keeps the
     * integral finite, an infinite one would pin the output for good */
    float integral = clampf(ctx->integral + error * dt, ctx->cfg.integral_min,
                            ctx->cfg.integral_max);
    if (is_finite(integral))
    {
        ctx->integral = integral;
    }

    float derivative = 0.0f;
    if (!ctx->first_run)
    {
        derivative = (error - ctx->prev_error) / dt;
    }
    ctx->first_run = false;
    ctx->prev_error = error;

    float output = ctx->cfg.kp * error
                 + ctx->cfg.ki * ctx->integral
                 + ctx->cfg.kd * derivative;

    return clampf(output, ctx->cfg.output_min, ctx->cfg.output_max);
}

pid_controller_err_t pid_controller_set_tunings(pid_controller_t *ctx,
                                                float kp, float ki, float kd)
{
    if (!ctx || !is_finite(kp) || !is_finite(ki) || !is_finite(kd))
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->cfg.kp = kp;
    ctx->cfg.ki = ki;
    ctx->cfg.kd = kd;
    return PID_CONTROLLER_OK;
}

pid_controller_err_t pid_controller_set_output_limits(pid_controller_t *ctx,
                                                      float min, float max)
{
    if (!ctx)
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    /* written so that NaN fails too */
    if (!(min <= max))
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->cfg.output_min = min;
    ctx->cfg.output_max = max;
    return PID_CONTROLLER_OK;
}
