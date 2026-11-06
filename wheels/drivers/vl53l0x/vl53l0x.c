// SPDX-License-Identifier: MIT
/*
 * vl53l0x — ST VL53L0X I2C time-of-flight distance sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Simplified init sequence derived from ST VL53L0X datasheet and
 * application notes. Not the full ST API — covers single-shot ranging
 * with configurable timing budget.
 */

#include "vl53l0x.h"
#include "contract.h"
#include <string.h>

/* ---- register map (subset) ---- */

#define REG_MODEL_ID                0xC0u
#define REG_SYSRANGE_START          0x00u
#define REG_RESULT_RANGE_STATUS     0x14u
#define REG_SYSTEM_INTERRUPT_CLEAR  0x0Bu
#define REG_SYSTEM_SEQUENCE_CONFIG  0x01u
#define REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70u
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_HI  0x71u
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_LO  0x72u
#define REG_MSRC_CONFIG_CONTROL     0x60u

/* ---- I2C helpers ---- */

static bool write_reg(const vl53l0x_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const vl53l0x_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static bool read_reg(const vl53l0x_t *dev, uint8_t reg, uint8_t *val)
{
    return read_regs(dev, reg, val, 1);
}

/* ---- timing budget encoding ---- */

/* VL53L0X_calc_macro_period_ps: 2304 VCSEL clocks of 1655 ps. The register
 * holds the VCSEL period as pclks / 2 - 1 (VL53L0X_decode_vcsel_period). */
static uint32_t macro_period_ns(uint8_t vcsel_reg)
{
    uint32_t pclks = ((uint32_t)vcsel_reg + 1u) << 1;
    return (2304u * pclks * 1655u + 500u) / 1000u;
}

/* VL53L0X_encode_timeout: (LSByte * 2^MSByte) + 1 macro periods. The timing
 * budget is at least 20 ms, so mclks is never 0. */
static uint16_t encode_timeout(uint32_t mclks)
{
    uint32_t ls_byte = mclks - 1u;
    uint32_t ms_byte = 0u;

    while (ls_byte > 0xFFu)
    {
        ls_byte >>= 1;
        ms_byte++;
    }
    return (uint16_t)((ms_byte << 8) | ls_byte);
}

/*
 * Give the whole budget to the final range step. The full ST API takes the
 * time of the earlier sequence steps off the budget first, which needs their
 * timeouts read back, so a real measurement here takes a little longer.
 * The period is read, not assumed: this driver loads no tuning settings.
 */
static bool write_budget(const vl53l0x_t *dev, uint32_t budget_us)
{
    uint8_t vcsel_reg = 0;
    if (!read_reg(dev, REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD, &vcsel_reg))
    {
        return false;
    }

    uint32_t macro_ns = macro_period_ns(vcsel_reg);
    uint32_t mclks = (budget_us * 1000u + macro_ns / 2u) / macro_ns;
    uint16_t timeout = encode_timeout(mclks);

    return write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_HI,
                     (uint8_t)(timeout >> 8)) &&
           write_reg(dev, REG_FINAL_RANGE_CONFIG_TIMEOUT_LO,
                     (uint8_t)(timeout & 0xFFu));
}

/* ---- public API ---- */

/* The steps of init that talk to the chip */
static vl53l0x_err_t bring_up(const vl53l0x_t *dev)
{
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
    if (!write_budget(dev, dev->timing_budget_us))
    {
        return VL53L0X_ERR_I2C;
    }

    return VL53L0X_OK;
}

vl53l0x_err_t vl53l0x_init(vl53l0x_t *dev, const vl53l0x_i2c_t *i2c,
                            const vl53l0x_cfg_t *cfg)
{
    if (!dev)
    {
        return VL53L0X_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it. delay_ms is optional: read_range_mm tests it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
    {
        return VL53L0X_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs)
    {
        return VL53L0X_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->timing_budget_us = 30000u; /* 30 ms default */

    vl53l0x_err_t err = bring_up(dev);
    if (err != VL53L0X_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    return VL53L0X_OK;
}

vl53l0x_err_t vl53l0x_start_single(vl53l0x_t *dev)
{
    if (!dev || !dev->i2c)
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
    if (!dev || !dev->i2c || !ready)
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
    if (!dev || !dev->i2c || !range_mm)
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
            if (dev->i2c->delay_ms)
            {
                dev->i2c->delay_ms(dev->i2c->ctx, 1);
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
            ENSURE(range < 8190u);
            return VL53L0X_OK;
        }
    }

    return VL53L0X_ERR_TIMEOUT;
}

vl53l0x_err_t vl53l0x_set_measurement_timing_budget(vl53l0x_t *dev,
                                                     uint32_t budget_us)
{
    if (!dev || !dev->i2c)
    {
        return VL53L0X_ERR_PARAM;
    }
    if (budget_us < 20000u || budget_us > 500000u)
    {
        return VL53L0X_ERR_PARAM;
    }

    dev->timing_budget_us = budget_us;

    if (!write_budget(dev, budget_us))
    {
        return VL53L0X_ERR_I2C;
    }

    return VL53L0X_OK;
}
