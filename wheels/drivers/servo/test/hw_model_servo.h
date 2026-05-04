// SPDX-License-Identifier: MIT
/*
 * hw_model_servo.h — Signal decoder model for PWM servo driver
 *
 * NOT a register model. The servo driver outputs pulse widths via a
 * HAL callback. This model captures those pulse values and provides
 * helpers to convert pulse width back to angle for verification.
 */

#ifndef HW_MODEL_SERVO_H
#define HW_MODEL_SERVO_H

#include "servo.h"
#include <string.h>

#define SERVO_MODEL_MAX_HISTORY  64

typedef struct {
    /* capture state */
    uint32_t pulse_history[SERVO_MODEL_MAX_HISTORY];
    uint32_t history_count;
    uint32_t last_pulse_us;
    int      hal_fail;
} servo_model_t;

static void servo_model_init(servo_model_t *m)
{
    memset(m, 0, sizeof(*m));
}

/* --- HAL callback wired to model --- */

static bool servo_model_set_pulse(void *ctx, uint32_t pulse_us)
{
    servo_model_t *m = (servo_model_t *)ctx;
    if (m->hal_fail) return false;
    m->last_pulse_us = pulse_us;
    if (m->history_count < SERVO_MODEL_MAX_HISTORY) {
        m->pulse_history[m->history_count++] = pulse_us;
    }
    return true;
}

static servo_pwm_t servo_model_pwm(servo_model_t *m)
{
    servo_pwm_t pwm;
    pwm.set_pulse_us = servo_model_set_pulse;
    pwm.ctx           = m;
    return pwm;
}

/* --- Query API --- */

static uint32_t servo_model_get_pulse_us(const servo_model_t *m)
{
    return m->last_pulse_us;
}

static uint32_t servo_model_get_history_count(const servo_model_t *m)
{
    return m->history_count;
}

static uint32_t servo_model_get_pulse_at(const servo_model_t *m, uint32_t idx)
{
    if (idx < m->history_count) {
        return m->pulse_history[idx];
    }
    return 0;
}

/*
 * Convert captured pulse width to angle in degrees (integer).
 * Uses the same math as servo_set_angle but in reverse:
 *   angle = (pulse - min_us) * range_deg / (max_us - min_us)
 */
static uint16_t servo_model_get_angle(const servo_model_t *m,
                                       uint16_t min_us,
                                       uint16_t max_us,
                                       uint16_t range_deg)
{
    uint32_t pulse = m->last_pulse_us;
    if (pulse <= min_us) return 0;
    if (pulse >= max_us) return range_deg;
    uint32_t span = (uint32_t)max_us - (uint32_t)min_us;
    return (uint16_t)(((pulse - min_us) * (uint32_t)range_deg) / span);
}

/*
 * Check if pulse is proportional to expected angle.
 * Returns expected pulse for the given angle so caller can compare.
 */
static uint32_t servo_model_expected_pulse(uint16_t angle,
                                            uint16_t min_us,
                                            uint16_t max_us,
                                            uint16_t range_deg)
{
    if (angle >= range_deg) return max_us;
    uint32_t span = (uint32_t)max_us - (uint32_t)min_us;
    return (uint32_t)min_us + (span * (uint32_t)angle) / (uint32_t)range_deg;
}

#endif /* HW_MODEL_SERVO_H */
