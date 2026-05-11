// SPDX-License-Identifier: MIT
/*
 * bno055 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "bno055.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

bno055_err_t bno055_init(bno055_t *dev,
                               const bno055_i2c_t *i2c)
{
    if (!dev || !i2c) return BNO055_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return BNO055_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, BNO055_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return BNO055_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return BNO055_OK;
}

bno055_err_t bno055_read(bno055_t *dev,
                               bno055_data_t *out)
{
    if (!dev || !out) return BNO055_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return BNO055_ERR_I2C;
}
