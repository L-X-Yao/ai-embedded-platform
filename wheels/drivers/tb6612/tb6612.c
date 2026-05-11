// SPDX-License-Identifier: MIT
/*
 * tb6612 -- Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal + pwm_hal (injected)
 * Instances: multi
 */

#include "tb6612.h"
#include "contract.h"

/* ---------- helpers ---------- */

static tb6612_err_t validate_dev_ch(const tb6612_t *dev,
                                    tb6612_channel_t ch)
{
    if (!dev || !dev->hal) return TB6612_ERR_PARAM;
    if (!dev->hal->write_pin || !dev->hal->set_pwm_duty)
        return TB6612_ERR_PARAM;
    if (ch != TB6612_MOTOR_A && ch != TB6612_MOTOR_B)
        return TB6612_ERR_PARAM;
    return TB6612_OK;
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Get IN1/IN2 pin IDs and PWM channel for the given motor channel */
static void get_channel_pins(const tb6612_t *dev, tb6612_channel_t ch,
                             uint8_t *in1, uint8_t *in2, uint8_t *pwm)
{
    if (ch == TB6612_MOTOR_A) {
        *in1 = dev->pins.ain1;
        *in2 = dev->pins.ain2;
        *pwm = dev->pins.pwma;
    } else {
        *in1 = dev->pins.bin1;
        *in2 = dev->pins.bin2;
        *pwm = dev->pins.pwmb;
    }
}

/* ---------- public API ---------- */

tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_hal_t *hal,
                         const tb6612_pins_t *pins, uint16_t max_duty)
{
    if (!dev || !hal || !pins) return TB6612_ERR_PARAM;
    if (!hal->write_pin || !hal->set_pwm_duty) return TB6612_ERR_PARAM;
    if (max_duty == 0) return TB6612_ERR_PARAM;

    dev->hal = hal;
    dev->pins = *pins;
    dev->max_duty = max_duty;

    /* Start in standby (STBY LOW = disabled) */
    hal->write_pin(hal->ctx, pins->stby, false);

    /* All direction pins LOW, PWM=0 */
    hal->write_pin(hal->ctx, pins->ain1, false);
    hal->write_pin(hal->ctx, pins->ain2, false);
    hal->write_pin(hal->ctx, pins->bin1, false);
    hal->write_pin(hal->ctx, pins->bin2, false);
    hal->set_pwm_duty(hal->ctx, pins->pwma, 0);
    hal->set_pwm_duty(hal->ctx, pins->pwmb, 0);

    ENSURE(dev->hal != NULL);
    return TB6612_OK;
}

tb6612_err_t tb6612_set_speed(tb6612_t *dev, tb6612_channel_t channel,
                              int16_t speed_permille)
{
    tb6612_err_t err = validate_dev_ch(dev, channel);
    if (err != TB6612_OK) return err;

    speed_permille = clamp_i16(speed_permille, -1000, 1000);

    uint8_t in1, in2, pwm_ch;
    get_channel_pins(dev, channel, &in1, &in2, &pwm_ch);

    const tb6612_hal_t *hal = dev->hal;

    if (speed_permille > 0) {
        /* CW: IN1=H, IN2=L */
        hal->write_pin(hal->ctx, in1, true);
        hal->write_pin(hal->ctx, in2, false);
        uint16_t duty = (uint16_t)((uint32_t)speed_permille *
                                   dev->max_duty / 1000U);
        hal->set_pwm_duty(hal->ctx, pwm_ch, duty);
    } else if (speed_permille < 0) {
        /* CCW: IN1=L, IN2=H */
        hal->write_pin(hal->ctx, in1, false);
        hal->write_pin(hal->ctx, in2, true);
        uint16_t duty = (uint16_t)((uint32_t)(-speed_permille) *
                                   dev->max_duty / 1000U);
        hal->set_pwm_duty(hal->ctx, pwm_ch, duty);
    } else {
        /* Stop: IN1=L, IN2=L, PWM=0 */
        hal->write_pin(hal->ctx, in1, false);
        hal->write_pin(hal->ctx, in2, false);
        hal->set_pwm_duty(hal->ctx, pwm_ch, 0);
    }
    return TB6612_OK;
}

tb6612_err_t tb6612_brake(tb6612_t *dev, tb6612_channel_t channel)
{
    tb6612_err_t err = validate_dev_ch(dev, channel);
    if (err != TB6612_OK) return err;

    uint8_t in1, in2, pwm_ch;
    get_channel_pins(dev, channel, &in1, &in2, &pwm_ch);

    const tb6612_hal_t *hal = dev->hal;
    /* Brake: IN1=H, IN2=H */
    hal->write_pin(hal->ctx, in1, true);
    hal->write_pin(hal->ctx, in2, true);
    hal->set_pwm_duty(hal->ctx, pwm_ch, dev->max_duty);
    return TB6612_OK;
}

tb6612_err_t tb6612_standby(tb6612_t *dev, bool on)
{
    if (!dev || !dev->hal || !dev->hal->write_pin) {
        return TB6612_ERR_PARAM;
    }
    /* on=true -> standby (STBY LOW), on=false -> active (STBY HIGH) */
    dev->hal->write_pin(dev->hal->ctx, dev->pins.stby, !on);
    return TB6612_OK;
}