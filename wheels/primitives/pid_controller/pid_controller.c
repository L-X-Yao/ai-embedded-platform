/*
 * pid_controller — discrete PID controller with anti-windup
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "pid_controller.h"

static float clampf(float val, float lo, float hi)
{
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
    if (cfg->output_min > cfg->output_max ||
        cfg->integral_min > cfg->integral_max)
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->cfg = *cfg;
    ctx->integral = 0.0f;
    ctx->prev_error = 0.0f;
    ctx->first_run = true;
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

    float error = setpoint - measured;

    ctx->integral += error * dt;
    ctx->integral = clampf(ctx->integral, ctx->cfg.integral_min,
                           ctx->cfg.integral_max);

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
    if (!ctx)
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
    if (min > max)
    {
        return PID_CONTROLLER_ERR_PARAM;
    }
    ctx->cfg.output_min = min;
    ctx->cfg.output_max = max;
    return PID_CONTROLLER_OK;
}
