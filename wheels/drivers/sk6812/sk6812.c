// SPDX-License-Identifier: MIT
/*
 * sk6812 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "sk6812.h"

sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_gpio_t *gpio)
{
    if (!dev || !gpio)
        return SK6812_ERR_PARAM;
    if (!gpio->set_output || !gpio->set_input || !gpio->write_pin ||
        !gpio->read_pin || !gpio->delay_us)
        return SK6812_ERR_PARAM;
    dev->gpio = gpio;
    return SK6812_OK;
}

/* TODO: implement read/write functions */
