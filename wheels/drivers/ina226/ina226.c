// SPDX-License-Identifier: MIT
/*
 * ina226 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "ina226.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

ina226_err_t ina226_init(ina226_t *dev,
                               const ina226_i2c_t *i2c)
{
    if (!dev || !i2c) return INA226_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return INA226_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, INA226_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return INA226_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return INA226_OK;
}

ina226_err_t ina226_read(ina226_t *dev,
                               ina226_data_t *out)
{
    if (!dev || !out) return INA226_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return INA226_ERR_I2C;
}
