// SPDX-License-Identifier: MIT
/*
 * a4988 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "a4988.h"

a4988_err_t a4988_init(a4988_t *dev, const a4988_gpio_t *gpio)
{
    if (!dev || !gpio)
        return A4988_ERR_PARAM;
    if (!gpio->set_output || !gpio->set_input || !gpio->write_pin ||
        !gpio->read_pin || !gpio->delay_us)
        return A4988_ERR_PARAM;
    dev->gpio = gpio;
    return A4988_OK;
}

/* TODO: implement read/write functions */
