// SPDX-License-Identifier: MIT
/*
 * adxl345 — Analog Devices ADXL345 3-axis digital accelerometer driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register sequences from Analog Devices ADXL345 datasheet (Rev. G).
 * 13-bit signed output, full-resolution mode, scale per range.
 */

#include "adxl345.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses (from datasheet Table 19) ---------- */

#define REG_DEVID        0x00u
#define REG_BW_RATE      0x2Cu
#define REG_POWER_CTL    0x2Du
#define REG_DATA_FORMAT  0x31u
#define REG_DATAX0       0x32u   /* 6 bytes: X0,X1,Y0,Y1,Z0,Z1 */

/* POWER_CTL bits */
#define POWER_CTL_MEASURE  0x08u

/* DATA_FORMAT bits */
#define DATA_FORMAT_FULL_RES  0x08u

/* Scale factors: full-resolution mode = 3.9 mg/LSB regardless of range.
 * Non-full-res: 2g=3.9, 4g=7.8, 8g=15.6, 16g=31.2 mg/LSB.
 * We use full-resolution mode. */
#define SCALE_FULL_RES_MG_PER_LSB  0.0039f

static bool write_reg(const adxl345_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const adxl345_t *dev, uint8_t reg, uint8_t *buf,
                      uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

adxl345_err_t adxl345_init(adxl345_t *dev, const adxl345_i2c_t *i2c,
                           const adxl345_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return ADXL345_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return ADXL345_ERR_PARAM;
    if ((unsigned)cfg->range > 3)
        return ADXL345_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->scale = SCALE_FULL_RES_MG_PER_LSB;

    /* Read and verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_DEVID, &id, 1))
        return ADXL345_ERR_I2C;
    if (id != ADXL345_CHIP_ID)
        return ADXL345_ERR_ID;

    /* Set data format: full resolution + range */
    uint8_t fmt = (uint8_t)(DATA_FORMAT_FULL_RES | (cfg->range & 0x03u));
    if (!write_reg(dev, REG_DATA_FORMAT, fmt))
        return ADXL345_ERR_I2C;

    /* Enable measurement mode */
    if (!write_reg(dev, REG_POWER_CTL, POWER_CTL_MEASURE))
        return ADXL345_ERR_I2C;

    INVARIANT(dev->i2c != NULL);
    return ADXL345_OK;
}

adxl345_err_t adxl345_read(adxl345_t *dev, adxl345_data_t *out)
{
    if (!dev || !out)
        return ADXL345_ERR_PARAM;

    uint8_t buf[6];
    if (!read_regs(dev, REG_DATAX0, buf, 6))
        return ADXL345_ERR_I2C;

    /* Each axis: 16-bit signed, little-endian (LSB first) */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->x_g = (float)raw_x * dev->scale;
    out->y_g = (float)raw_y * dev->scale;
    out->z_g = (float)raw_z * dev->scale;

    ENSURE(out->x_g == out->x_g);  /* not NaN */
    ENSURE(out->y_g == out->y_g);
    ENSURE(out->z_g == out->z_g);
    return ADXL345_OK;
}
