/*
 * servo — PWM servo motor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    pwm_hal (injected)
 */
#include "servo.h"

servo_err_t servo_init(servo_t *dev, const servo_pwm_t *pwm,
                       const servo_cfg_t *cfg)
{
    if (!dev || !pwm || !cfg)
        return SERVO_ERR_PARAM;
    if (!pwm->set_pulse_us)
        return SERVO_ERR_PARAM;
    if (cfg->min_pulse_us >= cfg->max_pulse_us || cfg->max_angle == 0)
        return SERVO_ERR_PARAM;
    dev->pwm = *pwm;
    dev->cfg = *cfg;
    dev->current_angle = 0;
    return SERVO_OK;
}

servo_err_t servo_set_angle(servo_t *dev, uint16_t angle)
{
    if (!dev)
        return SERVO_ERR_PARAM;
    if (angle > dev->cfg.max_angle)
        angle = dev->cfg.max_angle;

    uint32_t range = dev->cfg.max_pulse_us - dev->cfg.min_pulse_us;
    uint32_t pulse = dev->cfg.min_pulse_us +
                     (range * (uint32_t)angle) / dev->cfg.max_angle;

    if (!dev->pwm.set_pulse_us(dev->pwm.ctx, pulse))
        return SERVO_ERR_HAL;

    dev->current_angle = angle;
    return SERVO_OK;
}

servo_err_t servo_set_pulse(servo_t *dev, uint32_t pulse_us)
{
    if (!dev)
        return SERVO_ERR_PARAM;
    if (!dev->pwm.set_pulse_us(dev->pwm.ctx, pulse_us))
        return SERVO_ERR_HAL;
    return SERVO_OK;
}

uint16_t servo_get_angle(const servo_t *dev)
{
    if (!dev) return 0;
    return dev->current_angle;
}
