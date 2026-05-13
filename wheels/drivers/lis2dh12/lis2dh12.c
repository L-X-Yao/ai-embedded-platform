// SPDX-License-Identifier: MIT
/*
 * lis2dh12 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "lis2dh12.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

lis2dh12_err_t lis2dh12_init(lis2dh12_t *dev,
                               const lis2dh12_i2c_t *i2c)
{
    if (!dev || !i2c) return LIS2DH12_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return LIS2DH12_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, LIS2DH12_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return LIS2DH12_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return LIS2DH12_OK;
}

lis2dh12_err_t lis2dh12_read(lis2dh12_t *dev,
                               lis2dh12_data_t *out)
{
    if (!dev || !dev->i2c || !out) return LIS2DH12_ERR_PARAM;
    /* TODO: implement */
    return LIS2DH12_ERR_I2C;
}
