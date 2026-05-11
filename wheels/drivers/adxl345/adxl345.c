// SPDX-License-Identifier: MIT
/*
 * adxl345 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "adxl345.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

adxl345_err_t adxl345_init(adxl345_t *dev,
                               const adxl345_i2c_t *i2c)
{
    if (!dev || !i2c) return ADXL345_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return ADXL345_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, ADXL345_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return ADXL345_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return ADXL345_OK;
}

adxl345_err_t adxl345_read(adxl345_t *dev,
                               adxl345_data_t *out)
{
    if (!dev || !out) return ADXL345_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return ADXL345_ERR_I2C;
}
