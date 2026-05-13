// SPDX-License-Identifier: MIT
/*
 * bmm150 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "bmm150.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

bmm150_err_t bmm150_init(bmm150_t *dev,
                               const bmm150_i2c_t *i2c)
{
    if (!dev || !i2c) return BMM150_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return BMM150_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, BMM150_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return BMM150_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return BMM150_OK;
}

bmm150_err_t bmm150_read(bmm150_t *dev,
                               bmm150_data_t *out)
{
    if (!dev || !dev->i2c || !out) return BMM150_ERR_PARAM;
    /* TODO: implement */
    return BMM150_ERR_I2C;
}
