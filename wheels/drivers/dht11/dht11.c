// SPDX-License-Identifier: MIT
/*
 * dht11 — DHT11 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */
#include "dht11.h"
#include "contract.h"
#include <string.h>

/* The manual's figure asks for more than 18 ms. This leaves room for a delay_us
 * that runs a little short. */
#define START_LOW_US 20000U

dht11_err_t dht11_init(dht11_t *dev, const dht11_gpio_t *gpio)
{
    if (!dev)
        return DHT11_ERR_PARAM;

    /* dev->gpio stays NULL unless init succeeds: dht11_read refuses a device
     * without it */
    memset(dev, 0, sizeof(*dev));

    if (!gpio)
        return DHT11_ERR_PARAM;
    if (!gpio->set_output || !gpio->set_input || !gpio->write_pin ||
        !gpio->read_pin || !gpio->delay_us)
        return DHT11_ERR_PARAM;
    dev->gpio = gpio;
    return DHT11_OK;
}

static dht11_err_t wait_pin(dht11_t *dev, bool level, uint32_t timeout_us)
{
    while (timeout_us > 0) {
        if (dev->gpio->read_pin(dev->gpio->ctx) == level)
            return DHT11_OK;
        dev->gpio->delay_us(dev->gpio->ctx, 1);
        timeout_us--;
    }
    return DHT11_ERR_TIMEOUT;
}

dht11_err_t dht11_read(dht11_t *dev, dht11_data_t *out)
{
    if (!dev || !dev->gpio || !out)
        return DHT11_ERR_PARAM;

    uint8_t bytes[5] = {0};

    dev->gpio->set_output(dev->gpio->ctx);
    dev->gpio->write_pin(dev->gpio->ctx, false);
    dev->gpio->delay_us(dev->gpio->ctx, START_LOW_US);

    /* Release the bus and let the pull-up raise it: driving it high could fight
     * a sensor that answers soon, and the manual gives no time for that. */
    dev->gpio->set_input(dev->gpio->ctx);

    if (wait_pin(dev, false, 100) != DHT11_OK)
        return DHT11_ERR_TIMEOUT;
    if (wait_pin(dev, true, 100) != DHT11_OK)
        return DHT11_ERR_TIMEOUT;
    if (wait_pin(dev, false, 100) != DHT11_OK)
        return DHT11_ERR_TIMEOUT;

    for (int i = 0; i < 40; i++) {
        if (wait_pin(dev, true, 80) != DHT11_OK)
            return DHT11_ERR_TIMEOUT;

        dev->gpio->delay_us(dev->gpio->ctx, 40);

        if (dev->gpio->read_pin(dev->gpio->ctx)) {
            bytes[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
            if (wait_pin(dev, false, 50) != DHT11_OK)
                return DHT11_ERR_TIMEOUT;
        }
    }

    uint8_t checksum = (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]);
    if (checksum != bytes[4])
        return DHT11_ERR_CHECKSUM;

    out->humidity_int    = bytes[0];
    out->humidity_dec    = bytes[1];
    out->temperature_int = bytes[2];
    out->temperature_dec = bytes[3];
    return DHT11_OK;
}
