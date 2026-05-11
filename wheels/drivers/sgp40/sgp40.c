// SPDX-License-Identifier: MIT
/*
 * sgp40 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "sgp40.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

sgp40_err_t sgp40_init(sgp40_t *dev,
                               const sgp40_i2c_t *i2c)
{
    if (!dev || !i2c) return SGP40_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return SGP40_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, SGP40_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return SGP40_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return SGP40_OK;
}

sgp40_err_t sgp40_read(sgp40_t *dev,
                               sgp40_data_t *out)
{
    if (!dev || !out) return SGP40_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return SGP40_ERR_I2C;
}
