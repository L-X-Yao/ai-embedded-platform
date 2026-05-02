// SPDX-License-Identifier: MIT
/*
 * pcf8574 — I2C 8-bit GPIO expander driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * The PCF8574 has no register map. A single I2C write sets all 8 output
 * latches; a single I2C read returns the actual pin levels.
 */

#include "pcf8574.h"

pcf8574_err_t pcf8574_init(pcf8574_t *dev, const pcf8574_i2c_t *i2c,
                           uint8_t dev_addr)
{
    if (!dev || !i2c || !i2c->write || !i2c->read)
        return PCF8574_ERR_PARAM;

    dev->i2c          = i2c;
    dev->dev_addr     = dev_addr;
    dev->output_state = 0xFFu;   /* all high — quasi-bidirectional input */

    /* Push initial state to verify I2C link is alive */
    if (!i2c->write(i2c->ctx, dev_addr, &dev->output_state, 1))
        return PCF8574_ERR_I2C;

    return PCF8574_OK;
}

pcf8574_err_t pcf8574_write(pcf8574_t *dev, uint8_t value)
{
    if (!dev || !dev->i2c) return PCF8574_ERR_PARAM;

    if (!dev->i2c->write(dev->i2c->ctx, dev->dev_addr, &value, 1))
        return PCF8574_ERR_I2C;

    dev->output_state = value;
    return PCF8574_OK;
}

pcf8574_err_t pcf8574_read(pcf8574_t *dev, uint8_t *value)
{
    if (!dev || !dev->i2c || !value) return PCF8574_ERR_PARAM;

    if (!dev->i2c->read(dev->i2c->ctx, dev->dev_addr, value, 1))
        return PCF8574_ERR_I2C;

    return PCF8574_OK;
}

pcf8574_err_t pcf8574_write_pin(pcf8574_t *dev, uint8_t pin, bool level)
{
    if (!dev || !dev->i2c) return PCF8574_ERR_PARAM;
    if (pin > 7) return PCF8574_ERR_PARAM;

    uint8_t mask = (uint8_t)(1u << pin);
    uint8_t val  = dev->output_state;

    if (level)
        val |= mask;
    else
        val = (uint8_t)(val & (uint8_t)~mask);

    return pcf8574_write(dev, val);
}

pcf8574_err_t pcf8574_read_pin(pcf8574_t *dev, uint8_t pin, bool *level)
{
    if (!dev || !dev->i2c || !level) return PCF8574_ERR_PARAM;
    if (pin > 7) return PCF8574_ERR_PARAM;

    uint8_t port = 0;
    pcf8574_err_t err = pcf8574_read(dev, &port);
    if (err != PCF8574_OK) return err;

    *level = (port & (uint8_t)(1u << pin)) != 0;
    return PCF8574_OK;
}

pcf8574_err_t pcf8574_toggle_pin(pcf8574_t *dev, uint8_t pin)
{
    if (!dev || !dev->i2c) return PCF8574_ERR_PARAM;
    if (pin > 7) return PCF8574_ERR_PARAM;

    uint8_t val = (uint8_t)(dev->output_state ^ (uint8_t)(1u << pin));
    return pcf8574_write(dev, val);
}
