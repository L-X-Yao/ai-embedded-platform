// SPDX-License-Identifier: MIT
/*
 * bma400 — Bosch BMA400 ultra-low-power 3-axis accelerometer driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 12-bit signed acceleration data, configurable range and power mode.
 * Datasheet: BST-BMA400-DS000-02.
 */

#include "bma400.h"
#include "contract.h"
#include <string.h>

/* ---------- register map ---------- */
#define REG_CHIP_ID     0x00u
#define REG_ACC_X_LSB   0x04u   /* 6 bytes: XL,XH,YL,YH,ZL,ZH */
#define REG_ACC_CONFIG0  0x19u  /* power mode bits [1:0] */
#define REG_ACC_CONFIG1  0x1Au  /* range bits [7:6] */

#define ACC_RANGE_MASK       0xC0u
#define ACC_RANGE_SHIFT      6u
#define MODE_CHANGE_WAIT_MS  10u

/* ---------- sensitivity: LSB per g for each range ---------- */
static const float range_sensitivity[] = {
    1024.0f,    /* +/-2g  */
     512.0f,    /* +/-4g  */
     256.0f,    /* +/-8g  */
     128.0f,    /* +/-16g */
};

/* ---------- I2C helpers ---------- */

static bool write_reg(const bma400_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bma400_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

/* ---------- 12-bit sign extension ---------- */

static int16_t sign_extend_12(uint8_t lo, uint8_t hi)
{
    /* BMA400: lower 4 bits of hi byte are data (bits 11:8).
     * raw 12-bit unsigned = (hi & 0x0F) << 8 | lo.
     * If bit 11 is set, sign-extend by OR-ing 0xF000. */
    uint16_t raw = (uint16_t)((uint16_t)(hi & 0x0Fu) << 8 | (uint16_t)lo);
    if (raw & 0x0800u) {
        raw = (uint16_t)(raw | 0xF000u);
    }
    return (int16_t)raw;
}

/* ---------- public API ---------- */

/* ACC_CONFIG1 also holds OSR and ODR, so only the range bits may change. The
 * read back catches a write the chip dropped, which would otherwise leave every
 * reading scaled for the wrong range. */
static bma400_err_t write_range(const bma400_t *dev, bma400_range_t range)
{
    uint8_t config1 = 0;
    if (!read_regs(dev, REG_ACC_CONFIG1, &config1, 1))
        return BMA400_ERR_I2C;

    config1 = (uint8_t)((config1 & (uint8_t)~ACC_RANGE_MASK) |
                        ((unsigned)range << ACC_RANGE_SHIFT));
    if (!write_reg(dev, REG_ACC_CONFIG1, config1))
        return BMA400_ERR_I2C;

    uint8_t check = 0;
    if (!read_regs(dev, REG_ACC_CONFIG1, &check, 1))
        return BMA400_ERR_I2C;
    return (check == config1) ? BMA400_OK : BMA400_ERR_I2C;
}

static bma400_err_t write_power(const bma400_t *dev, bma400_power_t mode)
{
    uint8_t config0 = (uint8_t)((unsigned)mode & 0x03u);
    if (!write_reg(dev, REG_ACC_CONFIG0, config0))
        return BMA400_ERR_I2C;

    if (dev->i2c->delay_ms)
        dev->i2c->delay_ms(dev->i2c->ctx, MODE_CHANGE_WAIT_MS);
    return BMA400_OK;
}

/* The steps of init that talk to the chip */
static bma400_err_t bring_up(bma400_t *dev, const bma400_cfg_t *cfg)
{
    /* Verify chip identity */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return BMA400_ERR_I2C;
    if (id != BMA400_CHIP_ID)
        return BMA400_ERR_ID;

    /* Configure first: the datasheet allows little in low power mode */
    bma400_err_t err = write_range(dev, cfg->range);
    if (err != BMA400_OK)
        return err;

    err = write_power(dev, cfg->power);
    if (err != BMA400_OK)
        return err;

    dev->sensitivity = range_sensitivity[cfg->range & 0x03u];
    return BMA400_OK;
}

bma400_err_t bma400_init(bma400_t *dev, const bma400_i2c_t *i2c,
                         const bma400_cfg_t *cfg)
{
    if (!dev)
        return BMA400_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return BMA400_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return BMA400_ERR_PARAM;
    if ((unsigned)cfg->range > 3 || (unsigned)cfg->power > 2)
        return BMA400_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    bma400_err_t err = bring_up(dev, cfg);
    if (err != BMA400_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return BMA400_OK;
}

bma400_err_t bma400_read(bma400_t *dev, bma400_data_t *out)
{
    if (!dev || !out)
        return BMA400_ERR_PARAM;
    if (!dev->i2c || dev->sensitivity <= 0.0f)
        return BMA400_ERR_PARAM;

    /* Sleep mode produces no data, the registers would only hold old samples */
    if (dev->cfg.power == BMA400_POWER_SLEEP)
        return BMA400_ERR_PARAM;

    uint8_t buf[6];
    if (!read_regs(dev, REG_ACC_X_LSB, buf, 6))
        return BMA400_ERR_I2C;

    int16_t rx = sign_extend_12(buf[0], buf[1]);
    int16_t ry = sign_extend_12(buf[2], buf[3]);
    int16_t rz = sign_extend_12(buf[4], buf[5]);

    /* Convert to milli-g: raw / sensitivity * 1000 */
    float inv_sens = 1000.0f / dev->sensitivity;
    out->x_mg = (float)rx * inv_sens;
    out->y_mg = (float)ry * inv_sens;
    out->z_mg = (float)rz * inv_sens;

    ENSURE(out->x_mg == out->x_mg);  /* not NaN */
    ENSURE(out->y_mg == out->y_mg);
    ENSURE(out->z_mg == out->z_mg);
    return BMA400_OK;
}

bma400_err_t bma400_set_power(bma400_t *dev, bma400_power_t mode)
{
    if (!dev || (unsigned)mode > 2)
        return BMA400_ERR_PARAM;
    if (!dev->i2c)
        return BMA400_ERR_PARAM;

    bma400_err_t err = write_power(dev, mode);
    if (err != BMA400_OK)
        return err;

    dev->cfg.power = mode;
    return BMA400_OK;
}

bma400_err_t bma400_set_range(bma400_t *dev, bma400_range_t range)
{
    if (!dev || (unsigned)range > 3)
        return BMA400_ERR_PARAM;
    if (!dev->i2c)
        return BMA400_ERR_PARAM;

    bma400_err_t err = write_range(dev, range);
    if (err != BMA400_OK)
        return err;

    dev->cfg.range = range;
    dev->sensitivity = range_sensitivity[range & 0x03u];
    return BMA400_OK;
}
