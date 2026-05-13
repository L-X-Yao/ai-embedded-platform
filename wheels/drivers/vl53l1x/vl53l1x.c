// SPDX-License-Identifier: MIT
/*
 * vl53l1x — ST VL53L1X time-of-flight distance sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Simplified init + continuous ranging. The real ST ultra-lite driver
 * writes a large default-config blob; we do a minimal subset that
 * verifies the chip and enables basic ranging.
 */

#include "vl53l1x.h"
#include <string.h>

/* ---- register map (16-bit addresses, subset) ---- */

#define REG_MODEL_ID              0x010Fu
#define REG_MODULE_TYPE           0x0110u
#define REG_SYSTEM_START          0x0087u
#define REG_RESULT_RANGE_HI       0x0096u
#define REG_RESULT_RANGE_LO       0x0097u

/* ---- I2C helpers ---- */

static bool write_reg(const vl53l1x_t *dev, uint16_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_reg(const vl53l1x_t *dev, uint16_t reg, uint8_t *val)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, val, 1);
}

/* ---- public API ---- */

vl53l1x_err_t vl53l1x_init(vl53l1x_t *dev, const vl53l1x_i2c_t *i2c,
                            const vl53l1x_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return VL53L1X_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs)
    {
        return VL53L1X_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->ranging = false;

    /* Verify model ID (0x010F must read 0xEA) */
    uint8_t id = 0;
    if (!read_reg(dev, REG_MODEL_ID, &id))
    {
        return VL53L1X_ERR_I2C;
    }
    if (id != VL53L1X_MODEL_ID_VAL)
    {
        return VL53L1X_ERR_ID;
    }

    /* Verify module type (0x0110 must read 0xCC) */
    uint8_t mtype = 0;
    if (!read_reg(dev, REG_MODULE_TYPE, &mtype))
    {
        return VL53L1X_ERR_I2C;
    }
    if (mtype != VL53L1X_MODULE_TYPE_VAL)
    {
        return VL53L1X_ERR_ID;
    }

    return VL53L1X_OK;
}

vl53l1x_err_t vl53l1x_start_ranging(vl53l1x_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return VL53L1X_ERR_PARAM;
    }

    if (!write_reg(dev, REG_SYSTEM_START, 0x40))
    {
        return VL53L1X_ERR_I2C;
    }

    dev->ranging = true;
    return VL53L1X_OK;
}

vl53l1x_err_t vl53l1x_stop_ranging(vl53l1x_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return VL53L1X_ERR_PARAM;
    }

    if (!write_reg(dev, REG_SYSTEM_START, 0x00))
    {
        return VL53L1X_ERR_I2C;
    }

    dev->ranging = false;
    return VL53L1X_OK;
}

vl53l1x_err_t vl53l1x_read_distance(vl53l1x_t *dev, uint16_t *dist_mm)
{
    if (!dev || !dev->i2c || !dist_mm)
    {
        return VL53L1X_ERR_PARAM;
    }

    /* Read MSB from 0x0096 */
    uint8_t msb = 0;
    if (!read_reg(dev, REG_RESULT_RANGE_HI, &msb))
    {
        return VL53L1X_ERR_I2C;
    }

    /* Read LSB from 0x0097 */
    uint8_t lsb = 0;
    if (!read_reg(dev, REG_RESULT_RANGE_LO, &lsb))
    {
        return VL53L1X_ERR_I2C;
    }

    *dist_mm = (uint16_t)((uint16_t)msb << 8 | lsb);
    return VL53L1X_OK;
}
