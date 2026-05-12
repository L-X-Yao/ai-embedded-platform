// SPDX-License-Identifier: MIT
/*
 * hcsr04 -- HC-SR04 ultrasonic distance sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal + timer_hal (injected)
 * Instances: multi
 *
 * Protocol: 10us trigger pulse -> echo pulse whose width = round-trip time.
 * Distance = width_us * 0.0343 / 2 cm (speed of sound ~343 m/s at 20C).
 * Timeout 25000us corresponds to ~4.3m max range.
 */

#include "hcsr04.h"
#include "contract.h"

/* Timeout in microseconds: ~4.3m max range + margin */
#define ECHO_TIMEOUT_US  25000u

/* Trigger pulse width in microseconds (datasheet: >= 10us) */
#define TRIGGER_PULSE_US 10u

/* Speed of sound conversion factor: cm per microsecond, one-way */
#define US_TO_CM_FACTOR  0.01715f   /* 0.0343 / 2 */

/* Valid range limits */
#define MIN_DISTANCE_CM  2.0f
#define MAX_DISTANCE_CM  400.0f

hcsr04_err_t hcsr04_init(hcsr04_t *dev, const hcsr04_hal_t *hal)
{
    if (!dev || !hal)
    {
        return HCSR04_ERR_PARAM;
    }

    if (!hal->trigger_high || !hal->trigger_low || !hal->echo_read ||
        !hal->delay_us || !hal->get_time_us)
    {
        return HCSR04_ERR_PARAM;
    }

    dev->hal = hal;

    INVARIANT(dev->hal != NULL);
    return HCSR04_OK;
}

hcsr04_err_t hcsr04_read_us(hcsr04_t *dev, uint32_t *echo_us)
{
    if (!dev || !echo_us)
    {
        return HCSR04_ERR_PARAM;
    }

    const hcsr04_hal_t *hal = dev->hal;

    /* Send 10us trigger pulse */
    hal->trigger_low(hal->ctx);
    hal->delay_us(hal->ctx, 2);
    hal->trigger_high(hal->ctx);
    hal->delay_us(hal->ctx, TRIGGER_PULSE_US);
    hal->trigger_low(hal->ctx);

    /* Wait for echo to go HIGH (start of return pulse) */
    uint32_t t0 = hal->get_time_us(hal->ctx);
    while (!hal->echo_read(hal->ctx))
    {
        uint32_t elapsed = hal->get_time_us(hal->ctx) - t0;
        if (elapsed >= ECHO_TIMEOUT_US)
        {
            return HCSR04_ERR_TIMEOUT;
        }
    }

    /* Measure echo HIGH duration */
    uint32_t echo_start = hal->get_time_us(hal->ctx);
    while (hal->echo_read(hal->ctx))
    {
        uint32_t elapsed = hal->get_time_us(hal->ctx) - echo_start;
        if (elapsed >= ECHO_TIMEOUT_US)
        {
            return HCSR04_ERR_TIMEOUT;
        }
    }

    uint32_t duration = hal->get_time_us(hal->ctx) - echo_start;
    *echo_us = duration;
    return HCSR04_OK;
}

hcsr04_err_t hcsr04_read_cm(hcsr04_t *dev, float *distance_cm)
{
    if (!dev || !distance_cm)
    {
        return HCSR04_ERR_PARAM;
    }

    uint32_t echo_us;
    hcsr04_err_t err = hcsr04_read_us(dev, &echo_us);
    if (err != HCSR04_OK)
    {
        return err;
    }

    float cm = (float)echo_us * US_TO_CM_FACTOR;

    /* NaN guard */
    ENSURE(cm == cm);

    if (cm < MIN_DISTANCE_CM || cm > MAX_DISTANCE_CM)
    {
        return HCSR04_ERR_RANGE;
    }

    *distance_cm = cm;
    return HCSR04_OK;
}
