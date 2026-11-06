// SPDX-License-Identifier: MIT
/*
 * adxl345 — Analog Devices ADXL345 3-axis accelerometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Full-resolution mode: 3.9 mg/LSB regardless of range setting.
 * Data from ADXL345 datasheet (Rev. E).
 */

#include "adxl345.h"
#include "contract.h"
#include <string.h>

#define REG_DEVID       0x00
#define REG_BW_RATE     0x2C
#define REG_POWER_CTL   0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0      0x32

/* POWER_CTL bits */
#define POWER_MEASURE   (1u << 3)

/* DATA_FORMAT bits */
#define DATA_FULL_RES   (1u << 3)

/* Full-resolution scale: 3.9 mg/LSB = 0.0039 g/LSB */
#define SCALE_FULL_RES  0.0039f

/* Turn-on time of each accepted rate code, rounded up to whole milliseconds.
 * Data sheet Rev. G, p.4, note 7: "approximately tau + 1.1 in milliseconds,
 * where tau = 1/(data rate)". */
static const uint16_t TURN_ON_MS[] = {
    10002, 5002, 2566, 1284, 643, 321, 162, 82, 42, 22, 12, 7, 4, 3,
};

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
    {
        return ADXL345_ERR_PARAM;
    }

    /* 800 Hz is the most a 400 kHz I2C bus can carry */
    if ((unsigned)cfg->range > 3 || (unsigned)cfg->data_rate > ADXL345_RATE_800HZ)
    {
        return ADXL345_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_DEVID, &id, 1))
    {
        return ADXL345_ERR_I2C;
    }
    if (id != ADXL345_CHIP_ID)
    {
        return ADXL345_ERR_ID;
    }

    /* Set data rate */
    if (!write_reg(dev, REG_BW_RATE, (uint8_t)(cfg->data_rate & 0x0F)))
    {
        return ADXL345_ERR_I2C;
    }

    /* Set data format: full resolution + range */
    uint8_t fmt = (uint8_t)(DATA_FULL_RES | (cfg->range & 0x03));
    if (!write_reg(dev, REG_DATA_FORMAT, fmt))
    {
        return ADXL345_ERR_I2C;
    }

    /* Enable measurement mode */
    if (!write_reg(dev, REG_POWER_CTL, POWER_MEASURE))
    {
        return ADXL345_ERR_I2C;
    }

    /* The data registers hold no sample until the turn-on time has passed */
    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, TURN_ON_MS[cfg->data_rate]);
    }

    INVARIANT(dev->i2c != NULL);
    return ADXL345_OK;
}

adxl345_err_t adxl345_read(const adxl345_t *dev, adxl345_data_t *out)
{
    if (!dev || !out)
    {
        return ADXL345_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_DATAX0, buf, 6))
    {
        return ADXL345_ERR_I2C;
    }

    /* Data is little-endian int16_t: X0,X1,Y0,Y1,Z0,Z1 */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->x_g = (float)raw_x * SCALE_FULL_RES;
    out->y_g = (float)raw_y * SCALE_FULL_RES;
    out->z_g = (float)raw_z * SCALE_FULL_RES;

    ENSURE(out->x_g == out->x_g);  /* not NaN */
    ENSURE(out->y_g == out->y_g);
    ENSURE(out->z_g == out->z_g);
    return ADXL345_OK;
}

adxl345_err_t adxl345_set_range(adxl345_t *dev, adxl345_range_t range)
{
    if (!dev)
    {
        return ADXL345_ERR_PARAM;
    }
    if ((unsigned)range > 3)
    {
        return ADXL345_ERR_PARAM;
    }

    uint8_t fmt = (uint8_t)(DATA_FULL_RES | (range & 0x03));
    if (!write_reg(dev, REG_DATA_FORMAT, fmt))
    {
        return ADXL345_ERR_I2C;
    }

    dev->cfg.range = range;
    return ADXL345_OK;
}
