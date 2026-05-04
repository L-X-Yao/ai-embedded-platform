// SPDX-License-Identifier: MIT
/*
 * bmp280 — Bosch BMP280 temperature and pressure sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithm from Bosch BMP280 datasheet (BST-BMP280-DS001-26),
 * Section 8.1 (integer 32-bit).
 */

#include "bmp280.h"
#include "contract.h"
#include <string.h>

#define REG_CALIB00    0x88
#define REG_ID         0xD0
#define REG_RESET      0xE0
#define REG_STATUS     0xF3
#define REG_CTRL_MEAS  0xF4
#define REG_CONFIG     0xF5
#define REG_PRESS_MSB  0xF7

#define RESET_CMD      0xB6
#define MODE_FORCED    0x01

static bool write_reg(const bmp280_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bmp280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static bmp280_err_t read_calibration(bmp280_t *dev)
{
    uint8_t buf[24];
    if (!read_regs(dev, REG_CALIB00, buf, 24))
    {
        return BMP280_ERR_I2C;
    }

    dev->calib.dig_t1 = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    dev->calib.dig_t2 = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    dev->calib.dig_t3 = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
    dev->calib.dig_p1 = (uint16_t)((uint16_t)buf[7] << 8 | buf[6]);
    dev->calib.dig_p2 = (int16_t)((uint16_t)buf[9] << 8 | buf[8]);
    dev->calib.dig_p3 = (int16_t)((uint16_t)buf[11] << 8 | buf[10]);
    dev->calib.dig_p4 = (int16_t)((uint16_t)buf[13] << 8 | buf[12]);
    dev->calib.dig_p5 = (int16_t)((uint16_t)buf[15] << 8 | buf[14]);
    dev->calib.dig_p6 = (int16_t)((uint16_t)buf[17] << 8 | buf[16]);
    dev->calib.dig_p7 = (int16_t)((uint16_t)buf[19] << 8 | buf[18]);
    dev->calib.dig_p8 = (int16_t)((uint16_t)buf[21] << 8 | buf[20]);
    dev->calib.dig_p9 = (int16_t)((uint16_t)buf[23] << 8 | buf[22]);

    return BMP280_OK;
}

bmp280_err_t bmp280_init(bmp280_t *dev, const bmp280_i2c_t *i2c,
                         const bmp280_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return BMP280_ERR_PARAM;
    }

    if ((unsigned)cfg->osrs_t > 5 || (unsigned)cfg->osrs_p > 5)
        return BMP280_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    uint8_t id = 0;
    if (!read_regs(dev, REG_ID, &id, 1))
    {
        return BMP280_ERR_I2C;
    }
    if (id != BMP280_CHIP_ID)
    {
        return BMP280_ERR_ID;
    }

    bmp280_err_t err = read_calibration(dev);
    if (err != BMP280_OK)
    {
        return err;
    }

    uint8_t config = 0x00;
    if (!write_reg(dev, REG_CONFIG, config))
    {
        return BMP280_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return BMP280_OK;
}

static int32_t compensate_temperature(bmp280_t *dev, int32_t adc_t)
{
    int32_t var1 = ((((adc_t >> 3) - (int32_t)((uint32_t)dev->calib.dig_t1 << 1)))
                    * ((int32_t)dev->calib.dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)dev->calib.dig_t1))
                      * ((adc_t >> 4) - ((int32_t)dev->calib.dig_t1))) >> 12)
                    * ((int32_t)dev->calib.dig_t3)) >> 14;
    dev->t_fine = var1 + var2;
    return (dev->t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(const bmp280_t *dev, int32_t adc_p)
{
    int32_t var1 = (((int32_t)dev->t_fine) >> 1) - 64000;
    int32_t var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dev->calib.dig_p6);
    var2 = var2 + ((var1 * ((int32_t)dev->calib.dig_p5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dev->calib.dig_p4) << 16);
    var1 = (((dev->calib.dig_p3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3)
            + ((((int32_t)dev->calib.dig_p2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dev->calib.dig_p1)) >> 15);

    if (var1 == 0)
    {
        return 0;
    }

    uint32_t p = (((uint32_t)(((int32_t)1048576) - adc_p) - (uint32_t)(var2 >> 12))) * 3125;
    if (p < 0x80000000u)
    {
        p = (p << 1) / ((uint32_t)var1);
    }
    else
    {
        p = (p / (uint32_t)var1) * 2;
    }

    var1 = (((int32_t)dev->calib.dig_p9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dev->calib.dig_p8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dev->calib.dig_p7) >> 4));

    return p;
}

bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out)
{
    if (!dev || !out)
    {
        return BMP280_ERR_PARAM;
    }

    /* 0x07 mask is wider than the 3-bit osrs fields strictly require (max valid
     * value is 5), but safe: extra bits are silently ignored by the BMP280 and
     * the shift/OR cannot overflow a uint8_t. */
    uint8_t ctrl = (uint8_t)(((dev->cfg.osrs_t & 0x07) << 5) | ((dev->cfg.osrs_p & 0x07) << 2) | MODE_FORCED);
    if (!write_reg(dev, REG_CTRL_MEAS, ctrl))
    {
        return BMP280_ERR_I2C;
    }

    for (int tries = 0; tries < 500; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_STATUS, &status, 1))
        {
            return BMP280_ERR_I2C;
        }
        if ((status & 0x08) == 0)
        {
            break;
        }
        if (tries == 499)
        {
            return BMP280_ERR_TIMEOUT;
        }
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_PRESS_MSB, buf, 6))
    {
        return BMP280_ERR_I2C;
    }

    int32_t adc_p = (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
    int32_t adc_t = (int32_t)(((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4));

    int32_t temp_raw = compensate_temperature(dev, adc_t);
    out->temperature_c = (float)temp_raw / 100.0f;

    uint32_t press_raw = compensate_pressure(dev, adc_p);
    out->pressure_pa = (float)press_raw;

    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */
    ENSURE(out->pressure_pa == out->pressure_pa);        /* not NaN */
    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 85.0f);
    ENSURE(out->pressure_pa >= 30000.0f && out->pressure_pa <= 110000.0f);
    return BMP280_OK;
}

bmp280_err_t bmp280_reset(bmp280_t *dev)
{
    if (!dev)
    {
        return BMP280_ERR_PARAM;
    }
    if (!write_reg(dev, REG_RESET, RESET_CMD))
    {
        return BMP280_ERR_I2C;
    }
    return BMP280_OK;
}
