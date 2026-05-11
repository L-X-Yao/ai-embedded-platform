// SPDX-License-Identifier: MIT
/*
 * tb6612 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "tb6612.h"

tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_gpio_t *gpio)
{
    if (!dev || !gpio)
        return TB6612_ERR_PARAM;
    if (!gpio->set_output || !gpio->set_input || !gpio->write_pin ||
        !gpio->read_pin || !gpio->delay_us)
        return TB6612_ERR_PARAM;
    dev->gpio = gpio;
    return TB6612_OK;
}

/* TODO: implement read/write functions */
