// SPDX-License-Identifier: MIT
/*
 * tmp117 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "tmp117.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

tmp117_err_t tmp117_init(tmp117_t *dev,
                               const tmp117_i2c_t *i2c)
{
    if (!dev || !i2c) return TMP117_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return TMP117_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, TMP117_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return TMP117_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return TMP117_OK;
}

tmp117_err_t tmp117_read(tmp117_t *dev,
                               tmp117_data_t *out)
{
    if (!dev || !dev->i2c || !out) return TMP117_ERR_PARAM;
    /* TODO: implement */
    return TMP117_ERR_I2C;
}
