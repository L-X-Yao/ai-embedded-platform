// SPDX-License-Identifier: MIT
/*
 * bno055 — Bosch BNO055 9-axis IMU with sensor fusion driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map and mode-switch timing from BNO055 datasheet (BST-BNO055-DS000).
 */

#include "bno055.h"
#include "contract.h"
#include <string.h>

/* Registers */
#define REG_CHIP_ID       0x00
#define REG_OPR_MODE      0x3D
#define REG_EUL_HEADING   0x1A
#define REG_CALIB_STAT    0x35

/* Euler scale: 1/16 degree per LSB */
#define EULER_SCALE       (1.0f / 16.0f)

/* Mode switch delays (ms) */
#define DELAY_TO_CONFIG_MS    19
#define DELAY_TO_ANY_MS       7
#define DELAY_FUSION_EXTRA_MS 600

static bool write_reg(const bno055_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bno055_t *dev, uint8_t reg, uint8_t *buf,
                      uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static void delay(const bno055_t *dev, uint32_t ms)
{
    dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

static bool is_fusion_mode(bno055_opr_mode_t mode)
{
    return mode >= BNO055_MODE_IMU && mode <= BNO055_MODE_NDOF;
}

static bno055_err_t switch_to_config(const bno055_t *dev)
{
    if (!write_reg(dev, REG_OPR_MODE, BNO055_MODE_CONFIG))
    {
        return BNO055_ERR_I2C;
    }
    delay(dev, DELAY_TO_CONFIG_MS);
    return BNO055_OK;
}

static bno055_err_t switch_to_mode(const bno055_t *dev, bno055_opr_mode_t mode)
{
    if (!write_reg(dev, REG_OPR_MODE, (uint8_t)mode))
    {
        return BNO055_ERR_I2C;
    }
    delay(dev, DELAY_TO_ANY_MS);
    if (is_fusion_mode(mode))
    {
        delay(dev, DELAY_FUSION_EXTRA_MS);
    }
    return BNO055_OK;
}

/*
 * Every change goes through CONFIG. dev->cfg.mode follows the chip one step at
 * a time, so after a failed write the fusion guard still sees the real mode:
 * outside the fusion modes the Euler registers read zero.
 */
static bno055_err_t change_mode(bno055_t *dev, bno055_opr_mode_t mode)
{
    bno055_err_t err = switch_to_config(dev);
    if (err != BNO055_OK)
    {
        return err;
    }
    dev->cfg.mode = BNO055_MODE_CONFIG;

    err = switch_to_mode(dev, mode);
    if (err != BNO055_OK)
    {
        return err;
    }
    dev->cfg.mode = mode;
    return BNO055_OK;
}

/* The steps of init that talk to the chip */
static bno055_err_t bring_up(bno055_t *dev, bno055_opr_mode_t mode)
{
    /* Verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
    {
        return BNO055_ERR_I2C;
    }
    if (id != BNO055_CHIP_ID)
    {
        return BNO055_ERR_ID;
    }

    return change_mode(dev, mode);
}

bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg)
{
    if (!dev)
    {
        return BNO055_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a device
     * without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
    {
        return BNO055_ERR_PARAM;
    }

    if (!i2c->write_reg || !i2c->read_regs)
    {
        return BNO055_ERR_PARAM;
    }

    /* Mode changes need their settling time, so the delay is not optional */
    if (!i2c->delay_ms)
    {
        return BNO055_ERR_PARAM;
    }

    if ((unsigned)cfg->mode > 0x0C)
    {
        return BNO055_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->cfg.mode = BNO055_MODE_CONFIG;  /* the mode after power-on */

    bno055_err_t err = bring_up(dev, cfg->mode);
    if (err != BNO055_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return BNO055_OK;
}

bno055_err_t bno055_read_euler(const bno055_t *dev, bno055_euler_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return BNO055_ERR_PARAM;
    }

    /* Orientation output only exists in the fusion modes */
    if (!is_fusion_mode(dev->cfg.mode))
    {
        return BNO055_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_EUL_HEADING, buf, 6))
    {
        return BNO055_ERR_I2C;
    }

    /* Data is little-endian int16_t: heading, roll, pitch */
    int16_t raw_heading = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_roll    = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_pitch   = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->heading_deg = (float)raw_heading * EULER_SCALE;
    out->roll_deg    = (float)raw_roll    * EULER_SCALE;
    out->pitch_deg   = (float)raw_pitch   * EULER_SCALE;

    ENSURE(out->heading_deg == out->heading_deg);  /* not NaN */
    ENSURE(out->roll_deg == out->roll_deg);
    ENSURE(out->pitch_deg == out->pitch_deg);
    return BNO055_OK;
}

bno055_err_t bno055_get_calib_status(const bno055_t *dev,
                                     bno055_calib_status_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return BNO055_ERR_PARAM;
    }

    uint8_t status = 0;
    if (!read_regs(dev, REG_CALIB_STAT, &status, 1))
    {
        return BNO055_ERR_I2C;
    }

    out->sys   = (uint8_t)((status >> 6) & 0x03);
    out->gyro  = (uint8_t)((status >> 4) & 0x03);
    out->accel = (uint8_t)((status >> 2) & 0x03);
    out->mag   = (uint8_t)(status & 0x03);

    return BNO055_OK;
}

bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opr_mode_t mode)
{
    if (!dev || !dev->i2c)
    {
        return BNO055_ERR_PARAM;
    }
    if ((unsigned)mode > 0x0C)
    {
        return BNO055_ERR_PARAM;
    }

    return change_mode(dev, mode);
}
