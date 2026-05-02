/*
 * vl53l0x — ST VL53L0X I2C time-of-flight distance sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Simplified init sequence derived from ST VL53L0X datasheet and
 * application notes. Not the full ST API — covers single-shot ranging
 * with configurable timing budget.
 */

#include "vl53l0x.h"
#include <string.h>

/* ---- register map (subset) ---- */

#define REG_MODEL_ID                0xC0u
#define REG_SYSRANGE_START          0x00u
#define REG_RESULT_RANGE_STATUS     0x14u
#define REG_SYSTEM_INTERRUPT_CLEAR  0x0Bu
#define REG_SYSTEM_SEQUENCE_CONFIG  0x01u
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_HI  0x71u
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_LO  0x72u
#define REG_MSRC_CONFIG_CONTROL     0x60u

/* ---- I2C helpers ---- */

static bool write_reg(const vl53l0x_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c.write_reg(dev->i2c.ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const vl53l0x_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c.read_regs(dev->i2c.ctx, dev->cfg.addr, reg, buf, len);
}

static bool read_reg(const vl53l0x_t *dev, uint8_t reg, uint8_t *val)
{
    return read_regs(dev, reg, val, 1);
}

/* ---- timing budget encoding ---- */

static uint16_t budget_to_timeout(uint32_t budget_us)
{
    /*
     * Simplified mapping: the full ST API computes elaborate VCSEL
     * period + macro period conversions. We use a linear approximation
     * that is good enough for the common 20-200 ms range.
     * Encoded timeout = budget_us / 2 (clamped to uint16_t).
     */
    uint32_t encoded = budget_us / 2u;
    if (encoded > 0xFFFFu)
    {
        encoded = 0xFFFFu;
    }
    return (uint16_t)encoded;
}

/* ---- public API ---- */

vl53l0x_err_t vl53l0x_init(vl53l0x_t *dev, const vl53l0x_i2c_t *i2c,
                            const vl53l0x_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return VL53L0X_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;
    dev->timing_budget_us = 30000u; /* 30 ms default */

    /* verify model ID */
    uint8_t id = 0;
    if (!read_reg(dev, REG_MODEL_ID, &id))
    {
        return VL53L0X_ERR_I2C;
    }
    if (id != VL53L0X_MODEL_ID)
    {
        return VL53L0X_ERR_ID;
    }

    /* simplified init: enable all sequence steps */
    if (!write_reg(dev, REG_SYSTEM_SEQUENCE_CONFIG, 0xFF))
    {
        return VL53L0X_ERR_I2C;
    }

    /* set MSRC config (min signal rate check) */
    if (!write_reg(dev, REG_MSRC_CONFIG_CONTROL, 0x12))
    {
        return VL53L0X_ERR_I2C;
    }

    /* apply default timing budget */
    uint16_t timeout = budget_to_timeout(dev->timing_budget_us);
    if (!write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_HI,
                   (uint8_t)(timeout >> 8)))
    {
        return VL53L0X_ERR_I2C;
    }
    if (!write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_LO,
                   (uint8_t)(timeout & 0xFFu)))
    {
        return VL53L0X_ERR_I2C;
    }

    return VL53L0X_OK;
}

vl53l0x_err_t vl53l0x_start_single(vl53l0x_t *dev)
{
    if (!dev)
    {
        return VL53L0X_ERR_PARAM;
    }

    /* clear interrupt */
    if (!write_reg(dev, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return VL53L0X_ERR_I2C;
    }

    /* start single-shot measurement */
    if (!write_reg(dev, REG_SYSRANGE_START, 0x01))
    {
        return VL53L0X_ERR_I2C;
    }

    return VL53L0X_OK;
}

vl53l0x_err_t vl53l0x_is_ready(vl53l0x_t *dev, bool *ready)
{
    if (!dev || !ready)
    {
        return VL53L0X_ERR_PARAM;
    }

    uint8_t status = 0;
    if (!read_reg(dev, REG_RESULT_RANGE_STATUS, &status))
    {
        return VL53L0X_ERR_I2C;
    }

    *ready = (status & 0x01u) != 0;
    return VL53L0X_OK;
}

vl53l0x_err_t vl53l0x_read_range_mm(vl53l0x_t *dev, uint16_t *range_mm)
{
    if (!dev || !range_mm)
    {
        return VL53L0X_ERR_PARAM;
    }

    /* start single-shot */
    vl53l0x_err_t err = vl53l0x_start_single(dev);
    if (err != VL53L0X_OK)
    {
        return err;
    }

    /* poll until ready or timeout */
    for (int tries = 0; tries < 500; tries++)
    {
        bool ready = false;
        err = vl53l0x_is_ready(dev, &ready);
        if (err != VL53L0X_OK)
        {
            return err;
        }
        if (!ready)
        {
            if (dev->i2c.delay_ms)
            {
                dev->i2c.delay_ms(dev->i2c.ctx, 1);
            }
            continue;
        }
        {
            /* read 16-bit range from RESULT_RANGE_STATUS+10 and +11
             * (registers 0x1E and 0x1F per datasheet) */
            uint8_t buf[2];
            if (!read_regs(dev, (uint8_t)(REG_RESULT_RANGE_STATUS + 10u),
                           buf, 2))
            {
                return VL53L0X_ERR_I2C;
            }
            uint16_t range = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);

            /* 8190 (0x1FFE) means no target detected / out of range */
            if (range >= 8190u)
            {
                return VL53L0X_ERR_RANGE;
            }

            *range_mm = range;
            return VL53L0X_OK;
        }
    }

    return VL53L0X_ERR_TIMEOUT;
}

vl53l0x_err_t vl53l0x_set_measurement_timing_budget(vl53l0x_t *dev,
                                                     uint32_t budget_us)
{
    if (!dev)
    {
        return VL53L0X_ERR_PARAM;
    }
    if (budget_us < 20000u || budget_us > 500000u)
    {
        return VL53L0X_ERR_PARAM;
    }

    dev->timing_budget_us = budget_us;

    uint16_t timeout = budget_to_timeout(budget_us);
    if (!write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_HI,
                   (uint8_t)(timeout >> 8)))
    {
        return VL53L0X_ERR_I2C;
    }
    if (!write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_LO,
                   (uint8_t)(timeout & 0xFFu)))
    {
        return VL53L0X_ERR_I2C;
    }

    return VL53L0X_OK;
}
