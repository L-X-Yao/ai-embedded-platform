// SPDX-License-Identifier: MIT
/*
 * tb6612 — Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal + pwm_hal (injected)
 * Instances: multi
 */

#include "tb6612.h"
#include "contract.h"
#include <string.h>

/* ---------- helpers ---------- */

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static tb6612_err_t validate_motor(const tb6612_t *dev, tb6612_motor_t motor)
{
    if (!dev || !dev->hal) return TB6612_ERR_PARAM;
    if (!dev->hal->write_pin || !dev->hal->set_pwm_duty) {
        return TB6612_ERR_PARAM;
    }
    if (motor != TB6612_MOTOR_A && motor != TB6612_MOTOR_B) {
        return TB6612_ERR_PARAM;
    }
    return TB6612_OK;
}

static void get_motor_pins(const tb6612_t *dev, tb6612_motor_t motor,
                           uint8_t *in1, uint8_t *in2, uint8_t *pwm_ch)
{
    if (motor == TB6612_MOTOR_A) {
        *in1    = dev->cfg.ain1;
        *in2    = dev->cfg.ain2;
        *pwm_ch = dev->cfg.pwm_ch_a;
    } else {
        *in1    = dev->cfg.bin1;
        *in2    = dev->cfg.bin2;
        *pwm_ch = dev->cfg.pwm_ch_b;
    }
}

/* ---------- public API ---------- */

tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_hal_t *hal,
                         const tb6612_cfg_t *cfg)
{
    if (!dev) return TB6612_ERR_PARAM;

    /* dev->hal stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!hal || !cfg) return TB6612_ERR_PARAM;
    if (!hal->write_pin || !hal->set_pwm_duty) return TB6612_ERR_PARAM;

    dev->hal = hal;
    dev->cfg = *cfg;
    dev->standby = true;

    /* Set STBY LOW (standby mode) */
    hal->write_pin(hal->ctx, cfg->stby, false);

    /* Set all direction pins LOW (coast) */
    hal->write_pin(hal->ctx, cfg->ain1, false);
    hal->write_pin(hal->ctx, cfg->ain2, false);
    hal->write_pin(hal->ctx, cfg->bin1, false);
    hal->write_pin(hal->ctx, cfg->bin2, false);

    /* PWM to zero */
    hal->set_pwm_duty(hal->ctx, cfg->pwm_ch_a, 0);
    hal->set_pwm_duty(hal->ctx, cfg->pwm_ch_b, 0);

    INVARIANT(dev->hal != NULL);
    return TB6612_OK;
}

tb6612_err_t tb6612_set_speed(tb6612_t *dev, tb6612_motor_t motor,
                              int16_t speed_permille)
{
    tb6612_err_t err;
    uint8_t in1, in2, pwm_ch;
    uint16_t duty;

    err = validate_motor(dev, motor);
    if (err != TB6612_OK) return err;

    speed_permille = clamp_i16(speed_permille, -1000, 1000);

    get_motor_pins(dev, motor, &in1, &in2, &pwm_ch);

    if (speed_permille > 0) {
        /* Forward: IN1=HIGH, IN2=LOW */
        dev->hal->write_pin(dev->hal->ctx, in1, true);
        dev->hal->write_pin(dev->hal->ctx, in2, false);
        duty = (uint16_t)speed_permille;
    } else if (speed_permille < 0) {
        /* Reverse: IN1=LOW, IN2=HIGH */
        dev->hal->write_pin(dev->hal->ctx, in1, false);
        dev->hal->write_pin(dev->hal->ctx, in2, true);
        duty = (uint16_t)(-speed_permille);
    } else {
        /* Coast: IN1=LOW, IN2=LOW */
        dev->hal->write_pin(dev->hal->ctx, in1, false);
        dev->hal->write_pin(dev->hal->ctx, in2, false);
        duty = 0;
    }

    dev->hal->set_pwm_duty(dev->hal->ctx, pwm_ch, duty);
    return TB6612_OK;
}

tb6612_err_t tb6612_brake(tb6612_t *dev, tb6612_motor_t motor)
{
    tb6612_err_t err;
    uint8_t in1, in2, pwm_ch;

    err = validate_motor(dev, motor);
    if (err != TB6612_OK) return err;

    get_motor_pins(dev, motor, &in1, &in2, &pwm_ch);

    /* Brake: both IN pins HIGH, PWM does not matter (can be max) */
    dev->hal->write_pin(dev->hal->ctx, in1, true);
    dev->hal->write_pin(dev->hal->ctx, in2, true);
    dev->hal->set_pwm_duty(dev->hal->ctx, pwm_ch, 1000);

    return TB6612_OK;
}

tb6612_err_t tb6612_standby(tb6612_t *dev, bool enable)
{
    if (!dev || !dev->hal) return TB6612_ERR_PARAM;
    if (!dev->hal->write_pin) return TB6612_ERR_PARAM;

    /* STBY=HIGH means active, STBY=LOW means standby */
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.stby, !enable);
    dev->standby = enable;

    return TB6612_OK;
}
