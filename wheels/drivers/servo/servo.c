// SPDX-License-Identifier: MIT
/*
 * servo — PWM servo motor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 */
#include "servo.h"
#include "contract.h"
#include <string.h>

servo_err_t servo_init(servo_t *dev, const servo_pwm_t *pwm,
                       const servo_cfg_t *cfg)
{
    if (!dev)
        return SERVO_ERR_PARAM;

    /* dev->pwm stays NULL unless init succeeds: the other calls refuse a
     * device without it, which also has no range to divide by */
    memset(dev, 0, sizeof(*dev));

    if (!pwm || !cfg)
        return SERVO_ERR_PARAM;
    if (!pwm->set_pulse_us)
        return SERVO_ERR_PARAM;
    if (cfg->min_pulse_us >= cfg->max_pulse_us || cfg->max_angle == 0)
        return SERVO_ERR_PARAM;
    dev->pwm = pwm;
    dev->cfg = *cfg;
    INVARIANT(dev->pwm != NULL);
    return SERVO_OK;
}

servo_err_t servo_set_angle(servo_t *dev, uint16_t angle)
{
    if (!dev || !dev->pwm)
        return SERVO_ERR_PARAM;
    if (angle > dev->cfg.max_angle)
        angle = dev->cfg.max_angle;

    uint32_t range = dev->cfg.max_pulse_us - dev->cfg.min_pulse_us;
    uint32_t pulse = dev->cfg.min_pulse_us +
                     (range * (uint32_t)angle) / dev->cfg.max_angle;

    if (!dev->pwm->set_pulse_us(dev->pwm->ctx, pulse))
        return SERVO_ERR_HAL;

    dev->current_angle = angle;
    return SERVO_OK;
}

servo_err_t servo_set_pulse(servo_t *dev, uint32_t pulse_us)
{
    if (!dev || !dev->pwm)
        return SERVO_ERR_PARAM;
    if (pulse_us < dev->cfg.min_pulse_us || pulse_us > dev->cfg.max_pulse_us)
        return SERVO_ERR_PARAM;
    if (!dev->pwm->set_pulse_us(dev->pwm->ctx, pulse_us))
        return SERVO_ERR_HAL;

    /* To the nearest degree: servo_set_angle truncates its pulse, so rounding
     * down here would read that pulse back one degree low */
    uint32_t range  = dev->cfg.max_pulse_us - dev->cfg.min_pulse_us;
    uint32_t offset = pulse_us - dev->cfg.min_pulse_us;
    dev->current_angle = (uint16_t)((offset * dev->cfg.max_angle + range / 2U) / range);
    return SERVO_OK;
}

uint16_t servo_get_angle(const servo_t *dev)
{
    if (!dev) return 0;
    return dev->current_angle;
}
