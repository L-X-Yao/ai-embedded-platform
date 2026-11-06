// SPDX-License-Identifier: MIT
/*
 * drv8833 — dual H-bridge DC motor driver (DRV8833/DRV8835)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 */

#include "drv8833.h"
#include "contract.h"
#include <string.h>

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

static int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---------- public API ---------- */

drv8833_err_t drv8833_init(drv8833_t *dev, const drv8833_hal_t *hal,
                           uint16_t max_duty)
{
    if (!dev)
        return DRV8833_ERR_PARAM;

    /* dev->hal stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!hal)
        return DRV8833_ERR_PARAM;
    if (!hal->set_pwm)
        return DRV8833_ERR_PARAM;
    if (max_duty == 0)
        return DRV8833_ERR_PARAM;

    dev->hal      = hal;
    dev->max_duty = max_duty;
    INVARIANT(dev->hal != NULL);
    return DRV8833_OK;
}

drv8833_err_t drv8833_set_speed(drv8833_t *dev, drv8833_channel_t ch,
                                int16_t speed)
{
    drv8833_err_t err = validate_dev_ch(dev, ch);
    if (err != DRV8833_OK)
        return err;

    /* In 32 bits: max_duty is a uint16_t, and speed is not */
    int32_t md   = (int32_t)dev->max_duty;
    int32_t duty = clamp((int32_t)speed, -md, md);

    uint8_t c = (uint8_t)ch;

    if (duty > 0) {
        /* forward: IN1=PWM, IN2=LOW */
        dev->hal->set_pwm(dev->hal->ctx, c, 0, (uint16_t)duty);
        dev->hal->set_pwm(dev->hal->ctx, c, 1, 0);
    } else if (duty < 0) {
        /* reverse: IN1=LOW, IN2=PWM */
        dev->hal->set_pwm(dev->hal->ctx, c, 0, 0);
        dev->hal->set_pwm(dev->hal->ctx, c, 1, (uint16_t)(-duty));
    } else {
        /* duty == 0 → coast */
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
