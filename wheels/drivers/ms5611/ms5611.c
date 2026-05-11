// SPDX-License-Identifier: MIT
/*
 * ms5611 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "ms5611.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

ms5611_err_t ms5611_init(ms5611_t *dev,
                               const ms5611_i2c_t *i2c)
{
    if (!dev || !i2c) return MS5611_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return MS5611_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, MS5611_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return MS5611_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return MS5611_OK;
}

ms5611_err_t ms5611_read(ms5611_t *dev,
                               ms5611_data_t *out)
{
    if (!dev || !out) return MS5611_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return MS5611_ERR_I2C;
}
