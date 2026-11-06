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
 * All integer math, no float.  In whole microseconds 2*delay/(4n+1)
 * truncates to zero long before the ramp ends, so the delay is held in
 * Q8 microseconds and the division remainder is carried to the next step,
 * as Atmel AVR446 does.  Delays stay near 1e6 us at most, so
 * 2*delay fits uint32_t in Q8.
 */

#include "stepper.h"
#include "contract.h"

#define RAMP_Q 8U   /* fractional bits of the ramp delay */

/* ---- helpers ---- */

/* 2*delay_q/den, with the remainder kept for the next step. */
static uint32_t ramp_term(uint32_t delay_q, uint32_t den, uint32_t *rem)
{
    uint32_t num = 2U * delay_q + *rem;
    *rem = num % den;
    return num / den;
}

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
    INVARIANT(s->hal != NULL);
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

    /* Note: when steps == INT32_MIN, (uint32_t)0 - (uint32_t)steps wraps to
     * 2147483648 which is correct unsigned magnitude.  This edge case is
     * theoretical (2 billion steps) but the arithmetic is well-defined. */
    uint32_t total = (steps > 0) ? (uint32_t)steps
                                      : (uint32_t)0 - (uint32_t)steps;
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
    bool triangle = (total <= 2 * n_accel);
    if (triangle) {
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
    uint32_t delay_q = c0 << RAMP_Q;
    uint32_t min_delay_q = min_delay << RAMP_Q;
    uint32_t rem = 0;

    for (uint32_t i = 0; i < total; i++) {
        /* determine phase */
        if (i < n_accel) {
            s->state = STEPPER_STATE_ACCEL;
            /* Austin formula: delay = delay * (4n - 1) / (4n + 1)
             * But during accel we reduce delay. The formula for step n:
             * c_n = c_{n-1} - 2*c_{n-1} / (4n + 1)
             */
            if (i > 0) {
                delay_q -= ramp_term(delay_q, 4U * i + 1U, &rem);
            }
            if (delay_q < min_delay_q) delay_q = min_delay_q;
        } else if (i >= decel_start) {
            s->state = STEPPER_STATE_DECEL;
            /* Mirror of accel: the same formula run backwards from
             * n = steps_left - 1, which is
             * c_{n-1} = c_n + 2*c_n / (4n - 1).
             * The last step has no delay after it.
             */
            uint32_t steps_left = total - i;
            if (steps_left > 1) {
                delay_q += ramp_term(delay_q, 4U * steps_left - 5U, &rem);
            }
        } else {
            s->state = STEPPER_STATE_CRUISE;
            /* An odd triangle has one step here: it holds the peak. */
            if (!triangle) delay_q = min_delay_q;
        }

        uint32_t delay = (delay_q + (1U << (RAMP_Q - 1U))) >> RAMP_Q;
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
