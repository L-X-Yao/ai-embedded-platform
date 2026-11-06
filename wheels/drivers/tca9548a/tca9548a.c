// SPDX-License-Identifier: MIT
/*
 * tca9548a — TCA9548A 8-channel I2C multiplexer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "tca9548a.h"
#include "contract.h"
#include <string.h>

/* TCA9548A has no register map: the control byte is the only byte after
 * the address, in a write and in a read (SCPS207H p16-p17).             */

/* ------------------------------------------------------------------ */

tca9548a_err_t tca9548a_init(tca9548a_t *dev,
                             const tca9548a_i2c_t *i2c,
                             uint8_t dev_addr)
{
    if (!dev)
        return TCA9548A_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !i2c->write_raw)
        return TCA9548A_ERR_PARAM;
    if (dev_addr < TCA9548A_ADDR_BASE || dev_addr > TCA9548A_ADDR_MAX)
        return TCA9548A_ERR_PARAM;

    dev->dev_addr = dev_addr;

    /* Disable all channels on init */
    const uint8_t none = 0x00;
    if (!i2c->write_raw(i2c->ctx, dev_addr, &none, 1))
        return TCA9548A_ERR_I2C;

    dev->i2c = i2c;
    dev->current_channel = 0x00;
    return TCA9548A_OK;
}

/* ------------------------------------------------------------------ */

tca9548a_err_t tca9548a_select(tca9548a_t *dev, uint8_t channel)
{
    if (!dev || !dev->i2c) return TCA9548A_ERR_PARAM;
    if (channel > TCA9548A_MAX_CHANNEL) return TCA9548A_ERR_PARAM;

    uint8_t mask = (uint8_t)(1u << channel);

    /* Skip I2C write if already on the right channel */
    if (dev->current_channel == mask) return TCA9548A_OK;

    if (!dev->i2c->write_raw(dev->i2c->ctx, dev->dev_addr, &mask, 1))
        return TCA9548A_ERR_I2C;

    dev->current_channel = mask;
    return TCA9548A_OK;
}

/* ------------------------------------------------------------------ */

tca9548a_err_t tca9548a_select_multi(tca9548a_t *dev, uint8_t mask)
{
    if (!dev || !dev->i2c) return TCA9548A_ERR_PARAM;

    if (dev->current_channel == mask) return TCA9548A_OK;

    if (!dev->i2c->write_raw(dev->i2c->ctx, dev->dev_addr, &mask, 1))
        return TCA9548A_ERR_I2C;

    dev->current_channel = mask;
    return TCA9548A_OK;
}

/* ------------------------------------------------------------------ */

tca9548a_err_t tca9548a_disable_all(tca9548a_t *dev)
{
    if (!dev || !dev->i2c) return TCA9548A_ERR_PARAM;

    if (dev->current_channel == 0x00) return TCA9548A_OK;

    const uint8_t none = 0x00;
    if (!dev->i2c->write_raw(dev->i2c->ctx, dev->dev_addr, &none, 1))
        return TCA9548A_ERR_I2C;

    dev->current_channel = 0x00;
    return TCA9548A_OK;
}

/* ------------------------------------------------------------------ */

tca9548a_err_t tca9548a_get_channel(tca9548a_t *dev, uint8_t *mask)
{
    if (!dev || !dev->i2c || !mask) return TCA9548A_ERR_PARAM;
    if (!dev->i2c->read_raw) return TCA9548A_ERR_PARAM;

    uint8_t val = 0;
    if (!dev->i2c->read_raw(dev->i2c->ctx, dev->dev_addr, &val, 1))
        return TCA9548A_ERR_I2C;

    *mask = val;
    dev->current_channel = val;   /* re-sync cache */
    return TCA9548A_OK;
}
