// SPDX-License-Identifier: MIT
/*
 * pcf8563 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "pcf8563.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

pcf8563_err_t pcf8563_init(pcf8563_t *dev,
                               const pcf8563_i2c_t *i2c)
{
    if (!dev || !i2c) return PCF8563_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return PCF8563_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, PCF8563_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return PCF8563_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_read(pcf8563_t *dev,
                               pcf8563_data_t *out)
{
    if (!dev || !out) return PCF8563_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return PCF8563_ERR_I2C;
}
