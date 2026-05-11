// SPDX-License-Identifier: MIT
/*
 * ccs811 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "ccs811.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

ccs811_err_t ccs811_init(ccs811_t *dev,
                               const ccs811_i2c_t *i2c)
{
    if (!dev || !i2c) return CCS811_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return CCS811_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, CCS811_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return CCS811_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return CCS811_OK;
}

ccs811_err_t ccs811_read(ccs811_t *dev,
                               ccs811_data_t *out)
{
    if (!dev || !out) return CCS811_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return CCS811_ERR_I2C;
}
