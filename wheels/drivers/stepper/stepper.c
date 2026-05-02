// SPDX-License-Identifier: MIT
/*
 * stepper — step/dir motor driver with trapezoidal acceleration
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Trapezoidal ramp uses David Austin's algorithm:
 *   delay_n = delay_{n-1} * (1 - 2 / (4n + 1))
 * All integer math, no float.
 */

#include "stepper.h"

/* ---- helpers ---- */

static void pulse_step(const stepper_hal_t *hal)
{
    hal->step_pin(hal->ctx, true);
    hal->delay_us(hal->ctx, 2);   /* minimum pulse width */
    hal->step_pin(hal->ctx, false);
}

static void set_direction(const stepper_hal_t *hal, stepper_dir_t dir)
{
    hal->dir_pin(hal->ctx, dir == STEPPER_DIR_CW ? false : true);
}

/* ---- public API ---- */

stepper_err_t stepper_init(stepper_t *s, const stepper_hal_t *hal,
                           uint16_t max_speed, uint16_t accel)
{
    if (!s || !hal)
        return STEPPER_ERR_PARAM;
    if (!hal->step_pin || !hal->dir_pin || !hal->delay_us)
        return STEPPER_ERR_PARAM;
    if (max_speed == 0 || accel == 0)
        return STEPPER_ERR_PARAM;

    s->hal = hal;
    s->max_speed = max_speed;
    s->accel = accel;
    s->position = 0;
    s->state = STEPPER_STATE_IDLE;
    s->step_count = 0;
    s->accel_steps = 0;
    s->decel_start = 0;
    s->total_steps = 0;
    s->current_delay_us = 0;
    return STEPPER_OK;
}

stepper_err_t stepper_move(stepper_t *s, int32_t steps)
{
    if (!s)
        return STEPPER_ERR_PARAM;
    if (steps == 0)
        return STEPPER_OK;

    /* direction */
    stepper_dir_t dir = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
    set_direction(s->hal, dir);

    uint32_t total = (steps > 0) ? (uint32_t)steps : (uint32_t)(-steps);
    int32_t  sign  = (steps > 0) ? 1 : -1;

    /* initial delay from acceleration:
     * first step delay = 0.676 * sqrt(2 / accel) seconds
     * In integer: delay_0 = 676000 * isqrt(2000000 / accel) / 1000  [us]
     * Simplified: delay_0 = 1000000 / sqrt(2 * accel)               [us]
     * Using integer approx: delay_0 = 676 * isqrt(2000000 / accel)  [us]
     */
    /* Simple approach: start from min_speed delay, accel toward max_speed.
     * min_delay = 1000000 / max_speed  (cruise speed)
     * initial_delay = 1000000 / sqrt(accel) * 0.676
     * We use a simpler formulation: c0 = 0.676 * sqrt(2/a) * 1e6 us
     * Integer approx: c0 = 676 * isqrt(2000000 / accel)
     */

    /* Integer square root (guard: 46340^2 = 2147395600 < INT32_MAX) */
    uint32_t val = 2000000UL / s->accel;
    uint32_t root = 1;
    while (root * root <= val) {
        if (root > 46340) break;  /* prevent uint32 overflow in root*root */
        root++;
    }
    root--;
    if (root == 0) root = 1;

    uint32_t c0 = 676UL * root;  /* initial delay in us */
    uint32_t min_delay = 1000000UL / s->max_speed;  /* cruise delay */

    if (c0 < min_delay) c0 = min_delay;

    /* steps needed to reach max speed */
    /* n_accel = max_speed^2 / (2 * accel) */
    uint32_t n_accel = ((uint32_t)s->max_speed * (uint32_t)s->max_speed)
                       / (2U * (uint32_t)s->accel);
    if (n_accel == 0) n_accel = 1;

    /* If total steps < 2*n_accel, it's a short move (triangle profile) */
    uint32_t decel_start;
    if (total <= 2 * n_accel) {
        /* triangle: accel for half, decel for half */
        n_accel = total / 2;
        decel_start = total - n_accel;
    } else {
        decel_start = total - n_accel;
    }

    /* store ramp state */
    s->total_steps = total;
    s->accel_steps = n_accel;
    s->decel_start = decel_start;
    s->step_count = 0;
    s->current_delay_us = c0;

    /* execute steps */
    uint32_t delay = c0;

    for (uint32_t i = 0; i < total; i++) {
        /* determine phase */
        if (i < n_accel) {
            s->state = STEPPER_STATE_ACCEL;
            /* Austin formula: delay = delay * (4n - 1) / (4n + 1)
             * But during accel we reduce delay. The formula for step n:
             * c_n = c_{n-1} - 2*c_{n-1} / (4n + 1)
             */
            if (i > 0) {
                delay = delay - (2U * delay) / (4U * i + 1U);
            }
            if (delay < min_delay) delay = min_delay;
        } else if (i >= decel_start) {
            s->state = STEPPER_STATE_DECEL;
            /* mirror of accel: steps remaining */
            uint32_t steps_left = total - i;
            if (steps_left > 0 && steps_left < n_accel) {
                /* increase delay (decelerate) */
                delay = delay + (2U * delay) / (4U * steps_left + 1U);
            }
        } else {
            s->state = STEPPER_STATE_CRUISE;
            delay = min_delay;
        }

        s->current_delay_us = delay;
        s->step_count = i + 1;

        pulse_step(s->hal);
        if (i < total - 1) {
            s->hal->delay_us(s->hal->ctx, delay);
        }
        s->position += sign;
    }

    s->state = STEPPER_STATE_IDLE;
    return STEPPER_OK;
}

stepper_err_t stepper_move_to(stepper_t *s, int32_t target)
{
    if (!s)
        return STEPPER_ERR_PARAM;
    return stepper_move(s, target - s->position);
}

void stepper_step_once(stepper_t *s, stepper_dir_t dir)
{
    if (!s) return;
    set_direction(s->hal, dir);
    pulse_step(s->hal);
    s->position += (dir == STEPPER_DIR_CW) ? 1 : -1;
}

int32_t stepper_get_position(const stepper_t *s)
{
    if (!s) return 0;
    return s->position;
}

void stepper_set_position(stepper_t *s, int32_t pos)
{
    if (!s) return;
    s->position = pos;
}

stepper_state_t stepper_get_state(const stepper_t *s)
{
    if (!s) return STEPPER_STATE_IDLE;
    return s->state;
}
