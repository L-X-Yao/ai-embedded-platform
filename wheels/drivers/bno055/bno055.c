// SPDX-License-Identifier: MIT
/*
 * bno055 — Bosch BNO055 9-axis IMU with on-chip sensor fusion
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register sequences from BST-BNO055-DS000 datasheet (Rev. 1.8).
 * Euler angles at 0x1A: heading/roll/pitch as int16 LE, 1/16 degree per LSB.
 */

#include "bno055.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses (from datasheet Table 4-2) ---------- */

#define REG_CHIP_ID          0x00u
#define REG_EUL_HEADING_LSB  0x1Au  /* 6 bytes: heading, roll, pitch (LE int16) */
#define REG_CALIB_STAT       0x35u
#define REG_OPR_MODE         0x3Du
#define REG_SYS_TRIGGER      0x3Fu

/* Mode switch delays from datasheet Table 3-6 */
#define MODE_SWITCH_DELAY_MS  600u  /* max for any mode → fusion mode */
#define CONFIG_MODE_DELAY_MS   20u

#define EULER_SCALE (1.0f / 16.0f)

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
    if (dev->i2c->delay_ms) {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return BNO055_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return BNO055_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Read and verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return BNO055_ERR_I2C;
    if (id != BNO055_CHIP_ID)
        return BNO055_ERR_ID;

    /* Ensure we are in CONFIG mode first */
    if (!write_reg(dev, REG_OPR_MODE, BNO055_MODE_CONFIG))
        return BNO055_ERR_I2C;
    delay(dev, CONFIG_MODE_DELAY_MS);

    /* Reset via SYS_TRIGGER */
    if (!write_reg(dev, REG_SYS_TRIGGER, 0x20))
        return BNO055_ERR_I2C;
    delay(dev, MODE_SWITCH_DELAY_MS);

    /* Verify chip ID again after reset */
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return BNO055_ERR_I2C;
    if (id != BNO055_CHIP_ID)
        return BNO055_ERR_ID;

    /* Switch to requested operation mode */
    if (!write_reg(dev, REG_OPR_MODE, (uint8_t)cfg->mode))
        return BNO055_ERR_I2C;
    delay(dev, MODE_SWITCH_DELAY_MS);

    INVARIANT(dev->i2c != NULL);
    return BNO055_OK;
}

bno055_err_t bno055_read_euler(bno055_t *dev, bno055_euler_t *out)
{
    if (!dev || !out)
        return BNO055_ERR_PARAM;

    uint8_t buf[6];
    if (!read_regs(dev, REG_EUL_HEADING_LSB, buf, 6))
        return BNO055_ERR_I2C;

    /* Each value: int16_t little-endian, scale 1/16 degree */
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

bno055_err_t bno055_read_calib_status(bno055_t *dev,
                                      bno055_calib_status_t *out)
{
    if (!dev || !out)
        return BNO055_ERR_PARAM;

    uint8_t stat = 0;
    if (!read_regs(dev, REG_CALIB_STAT, &stat, 1))
        return BNO055_ERR_I2C;

    out->sys   = (uint8_t)((stat >> 6) & 0x03);
    out->gyro  = (uint8_t)((stat >> 4) & 0x03);
    out->accel = (uint8_t)((stat >> 2) & 0x03);
    out->mag   = (uint8_t)(stat & 0x03);

    return BNO055_OK;
}

bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opmode_t mode)
{
    if (!dev)
        return BNO055_ERR_PARAM;

    if (!write_reg(dev, REG_OPR_MODE, (uint8_t)mode))
        return BNO055_ERR_I2C;

    delay(dev, MODE_SWITCH_DELAY_MS);
    dev->cfg.mode = mode;

    return BNO055_OK;
}
