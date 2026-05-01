/*
 * drv8833 — dual H-bridge DC motor driver (DRV8833/DRV8835)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    pwm_hal (injected)
 */

#include "drv8833.h"

/* ---------- helpers ---------- */

static drv8833_err_t validate_dev_ch(const drv8833_t *dev,
                                     drv8833_channel_t ch)
{
    if (!dev || !dev->hal || !dev->hal->set_pwm)
        return DRV8833_ERR_PARAM;
    if (ch != DRV8833_CH_A && ch != DRV8833_CH_B)
        return DRV8833_ERR_PARAM;
    return DRV8833_OK;
}

static int16_t clamp(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---------- public API ---------- */

drv8833_err_t drv8833_init(drv8833_t *dev, const drv8833_hal_t *hal,
                           uint16_t max_duty)
{
    if (!dev || !hal)
        return DRV8833_ERR_PARAM;
    if (!hal->set_pwm)
        return DRV8833_ERR_PARAM;
    if (max_duty == 0)
        return DRV8833_ERR_PARAM;

    dev->hal      = hal;
    dev->max_duty = max_duty;
    return DRV8833_OK;
}

drv8833_err_t drv8833_set_speed(drv8833_t *dev, drv8833_channel_t ch,
                                int16_t speed)
{
    drv8833_err_t err = validate_dev_ch(dev, ch);
    if (err != DRV8833_OK)
        return err;

    int16_t md = (int16_t)dev->max_duty;
    speed = clamp(speed, (int16_t)(-md), md);

    uint8_t c = (uint8_t)ch;

    if (speed > 0) {
        /* forward: IN1=PWM, IN2=LOW */
        dev->hal->set_pwm(dev->hal->ctx, c, 0, (uint16_t)speed);
        dev->hal->set_pwm(dev->hal->ctx, c, 1, 0);
    } else if (speed < 0) {
        /* reverse: IN1=LOW, IN2=PWM */
        dev->hal->set_pwm(dev->hal->ctx, c, 0, 0);
        dev->hal->set_pwm(dev->hal->ctx, c, 1, (uint16_t)(-speed));
    } else {
        /* speed == 0 → coast */
        dev->hal->set_pwm(dev->hal->ctx, c, 0, 0);
        dev->hal->set_pwm(dev->hal->ctx, c, 1, 0);
    }
    return DRV8833_OK;
}

drv8833_err_t drv8833_brake(drv8833_t *dev, drv8833_channel_t ch)
{
    drv8833_err_t err = validate_dev_ch(dev, ch);
    if (err != DRV8833_OK)
        return err;

    uint8_t c = (uint8_t)ch;
    dev->hal->set_pwm(dev->hal->ctx, c, 0, dev->max_duty);
    dev->hal->set_pwm(dev->hal->ctx, c, 1, dev->max_duty);
    return DRV8833_OK;
}

drv8833_err_t drv8833_coast(drv8833_t *dev, drv8833_channel_t ch)
{
    drv8833_err_t err = validate_dev_ch(dev, ch);
    if (err != DRV8833_OK)
        return err;

    uint8_t c = (uint8_t)ch;
    dev->hal->set_pwm(dev->hal->ctx, c, 0, 0);
    dev->hal->set_pwm(dev->hal->ctx, c, 1, 0);
    return DRV8833_OK;
}

void drv8833_sleep(drv8833_t *dev)
{
    if (dev && dev->hal && dev->hal->set_sleep)
        dev->hal->set_sleep(dev->hal->ctx, true);
}

void drv8833_wake(drv8833_t *dev)
{
    if (dev && dev->hal && dev->hal->set_sleep)
        dev->hal->set_sleep(dev->hal->ctx, false);
}
