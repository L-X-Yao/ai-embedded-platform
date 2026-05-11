// SPDX-License-Identifier: MIT
/*
 * sh1106 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "sh1106.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

sh1106_err_t sh1106_init(sh1106_t *dev,
                               const sh1106_i2c_t *i2c)
{
    if (!dev || !i2c) return SH1106_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return SH1106_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, SH1106_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return SH1106_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return SH1106_OK;
}

sh1106_err_t sh1106_read(sh1106_t *dev,
                               sh1106_data_t *out)
{
    if (!dev || !out) return SH1106_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return SH1106_ERR_I2C;
}
