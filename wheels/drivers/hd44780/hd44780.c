// SPDX-License-Identifier: MIT
/*
 * hd44780 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "hd44780.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

hd44780_err_t hd44780_init(hd44780_t *dev,
                               const hd44780_i2c_t *i2c)
{
    if (!dev || !i2c) return HD44780_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return HD44780_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, HD44780_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return HD44780_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return HD44780_OK;
}

hd44780_err_t hd44780_read(hd44780_t *dev,
                               hd44780_data_t *out)
{
    if (!dev || !out) return HD44780_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return HD44780_ERR_I2C;
}
