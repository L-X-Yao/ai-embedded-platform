// SPDX-License-Identifier: MIT
/*
 * a4988 — Allegro A4988 stepper motor driver (GPIO)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "a4988.h"
#include "contract.h"
#include <string.h>

/* Minimum step pulse width: 2us (datasheet minimum is 1us) */
#define STEP_PULSE_US  2U

/* Minimum STEP LOW time between pulses (datasheet tB) */
#define STEP_LOW_US    1U

/* MS1/MS2/MS3 truth table for microstepping modes:
 *   FULL:      000
 *   HALF:      100
 *   QUARTER:   010
 *   EIGHTH:    110
 *   SIXTEENTH: 111
 */
static const bool ms_table[5][3] = {
    /* MS1,   MS2,   MS3 */
    { false, false, false },  /* FULL */
    { true,  false, false },  /* HALF */
    { false, true,  false },  /* QUARTER */
    { true,  true,  false },  /* EIGHTH */
    { true,  true,  true  },  /* SIXTEENTH */
};

/* ---------- public API ---------- */

a4988_err_t a4988_init(a4988_t *dev, const a4988_hal_t *hal,
                       const a4988_cfg_t *cfg)
{
    if (!dev) return A4988_ERR_PARAM;

    /* dev->hal stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!hal || !cfg) return A4988_ERR_PARAM;
    if (!hal->write_pin || !hal->delay_us) return A4988_ERR_PARAM;

    dev->hal = hal;
    dev->cfg = *cfg;
    dev->direction = A4988_DIR_CW;
    dev->microstep = A4988_STEP_FULL;
    dev->enabled = false;

    /* ENABLE HIGH = disabled (active-LOW) */
    hal->write_pin(hal->ctx, cfg->enable, true);

    /* DIR LOW = CW default */
    hal->write_pin(hal->ctx, cfg->dir, false);

    /* STEP LOW */
    hal->write_pin(hal->ctx, cfg->step, false);

    /* Default: full-step mode */
    hal->write_pin(hal->ctx, cfg->ms1, false);
    hal->write_pin(hal->ctx, cfg->ms2, false);
    hal->write_pin(hal->ctx, cfg->ms3, false);

    INVARIANT(dev->hal != NULL);
    return A4988_OK;
}

a4988_err_t a4988_enable(a4988_t *dev, bool on)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;

    /* ENABLE is active-LOW: LOW=enabled, HIGH=disabled */
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.enable, !on);
    dev->enabled = on;

    return A4988_OK;
}

a4988_err_t a4988_set_direction(a4988_t *dev, a4988_dir_t dir)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;
    if (dir != A4988_DIR_CW && dir != A4988_DIR_CCW) {
        return A4988_ERR_PARAM;
    }

    dev->hal->write_pin(dev->hal->ctx, dev->cfg.dir, (bool)dir);
    dev->direction = dir;

    return A4988_OK;
}

a4988_err_t a4988_step(a4988_t *dev)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;

    /* Rising edge on STEP triggers one microstep */
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.step, true);
    dev->hal->delay_us(dev->hal->ctx, STEP_PULSE_US);
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.step, false);

    return A4988_OK;
}

a4988_err_t a4988_step_n(a4988_t *dev, uint32_t count, uint32_t delay_us_between)
{
    uint32_t i;
    a4988_err_t err;

    if (!dev || !dev->hal) return A4988_ERR_PARAM;

    for (i = 0; i < count; i++) {
        err = a4988_step(dev);
        if (err != A4988_OK) return err;

        if (i < count - 1) {
            dev->hal->delay_us(dev->hal->ctx,
                               delay_us_between > STEP_LOW_US
                                   ? delay_us_between : STEP_LOW_US);
        }
    }

    return A4988_OK;
}

a4988_err_t a4988_set_microstep(a4988_t *dev, a4988_microstep_t mode)
{
    if (!dev || !dev->hal) return A4988_ERR_PARAM;
    if (mode > A4988_STEP_SIXTEENTH) return A4988_ERR_PARAM;

    dev->hal->write_pin(dev->hal->ctx, dev->cfg.ms1, ms_table[mode][0]);
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.ms2, ms_table[mode][1]);
    dev->hal->write_pin(dev->hal->ctx, dev->cfg.ms3, ms_table[mode][2]);

    dev->microstep = mode;

    return A4988_OK;
}
