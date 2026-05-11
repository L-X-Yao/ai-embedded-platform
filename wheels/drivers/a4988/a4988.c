// SPDX-License-Identifier: MIT
/*
 * a4988 -- Allegro A4988 stepper motor driver (GPIO)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "a4988.h"
#include "contract.h"

/* Microstepping truth table: MS1, MS2, MS3 */
static const bool ms_table[5][3] = {
    /* FULL:      */ {false, false, false},
    /* HALF:      */ {true,  false, false},
    /* QUARTER:   */ {false, true,  false},
    /* EIGHTH:    */ {true,  true,  false},
    /* SIXTEENTH: */ {true,  true,  true},
};

/* Minimum step pulse width in microseconds */
#define STEP_PULSE_US 2

/* ---------- public API ---------- */

a4988_err_t a4988_init(a4988_t *dev, const a4988_hal_t *hal,
                       const a4988_pins_t *pins)
{
    if (!dev || !hal || !pins) return A4988_ERR_PARAM;
    if (!hal->write_pin || !hal->delay_us) return A4988_ERR_PARAM;

    dev->hal = hal;
    dev->pins = *pins;
    dev->enabled = false;
    dev->direction = A4988_DIR_CW;
    dev->microstep = A4988_STEP_FULL;

    /* ENABLE HIGH = disabled */
    hal->write_pin(hal->ctx, pins->enable, true);

    /* STEP LOW */
    hal->write_pin(hal->ctx, pins->step, false);

    /* DIR LOW = CW */
    hal->write_pin(hal->ctx, pins->dir, false);

    /* Set full-step mode */
    hal->write_pin(hal->ctx, pins->ms1, false);
    hal->write_pin(hal->ctx, pins->ms2, false);
    hal->write_pin(hal->ctx, pins->ms3, false);

    ENSURE(dev->hal != NULL);
    return A4988_OK;
}

a4988_err_t a4988_enable(a4988_t *dev, bool enable)
{
    if (!dev || !dev->hal || !dev->hal->write_pin) {
        return A4988_ERR_PARAM;
    }
    /* ENABLE: LOW=active, HIGH=disabled */
    dev->hal->write_pin(dev->hal->ctx, dev->pins.enable, !enable);
    dev->enabled = enable;
    return A4988_OK;
}

a4988_err_t a4988_set_direction(a4988_t *dev, a4988_dir_t dir)
{
    if (!dev || !dev->hal || !dev->hal->write_pin) {
        return A4988_ERR_PARAM;
    }
    if (dir != A4988_DIR_CW && dir != A4988_DIR_CCW) {
        return A4988_ERR_PARAM;
    }
    dev->hal->write_pin(dev->hal->ctx, dev->pins.dir,
                        (dir == A4988_DIR_CCW));
    dev->direction = dir;
    return A4988_OK;
}

a4988_err_t a4988_step(a4988_t *dev)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;
    if (!dev->hal->write_pin || !dev->hal->delay_us) {
        return A4988_ERR_PARAM;
    }

    const a4988_hal_t *hal = dev->hal;
    /* Rising edge = one step */
    hal->write_pin(hal->ctx, dev->pins.step, true);
    hal->delay_us(hal->ctx, STEP_PULSE_US);
    hal->write_pin(hal->ctx, dev->pins.step, false);
    return A4988_OK;
}

a4988_err_t a4988_step_n(a4988_t *dev, uint32_t steps,
                         uint32_t delay_us_between)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;
    if (!dev->hal->write_pin || !dev->hal->delay_us) {
        return A4988_ERR_PARAM;
    }

    for (uint32_t i = 0; i < steps; i++) {
        a4988_err_t err = a4988_step(dev);
        if (err != A4988_OK) return err;
        if (delay_us_between > 0 && i + 1 < steps) {
            dev->hal->delay_us(dev->hal->ctx, delay_us_between);
        }
    }
    return A4988_OK;
}

a4988_err_t a4988_set_microstep(a4988_t *dev, a4988_microstep_t mode)
{
    if (!dev || !dev->hal || !dev->hal->write_pin) {
        return A4988_ERR_PARAM;
    }
    if (mode > A4988_STEP_SIXTEENTH) {
        return A4988_ERR_PARAM;
    }

    const a4988_hal_t *hal = dev->hal;
    hal->write_pin(hal->ctx, dev->pins.ms1, ms_table[mode][0]);
    hal->write_pin(hal->ctx, dev->pins.ms2, ms_table[mode][1]);
    hal->write_pin(hal->ctx, dev->pins.ms3, ms_table[mode][2]);
    dev->microstep = mode;
    return A4988_OK;
}