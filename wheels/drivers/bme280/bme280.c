// SPDX-License-Identifier: MIT
/*
 * bme280 — Bosch BME280 temperature, humidity, and pressure sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithm from Bosch BME280 datasheet (BST-BME280-DS002-15),
 * Section 8.1 (integer 32-bit) for temp/pressure, Section 4.2.3 for humidity.
 */

#include "bme280.h"
#include <string.h>

#define REG_CALIB00     0x88
#define REG_CALIB_H1    0xA1
#define REG_ID          0xD0
#define REG_RESET       0xE0
#define REG_CALIB_H2    0xE1
#define REG_CTRL_HUM    0xF2
#define REG_STATUS      0xF3
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_PRESS_MSB   0xF7
#define REG_HUM_MSB     0xFD

#define RESET_CMD       0xB6
#define MODE_FORCED     0x01

static bool write_reg(const bme280_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c.write_reg(dev->i2c.ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bme280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c.read_regs(dev->i2c.ctx, dev->cfg.addr, reg, buf, len);
}

static bme280_err_t read_calibration(bme280_t *dev)
{
    /* Temperature and pressure calibration: registers 0x88..0xA1 (26 bytes) */
    uint8_t buf[26];
    if (!read_regs(dev, REG_CALIB00, buf, 26))
    {
        return BME280_ERR_I2C;
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

    /* dig_h1 at 0xA1 (1 byte) */
    dev->calib.dig_h1 = buf[25];

    /* Humidity calibration: registers 0xE1..0xE7 (7 bytes) */
    uint8_t hbuf[7];
    if (!read_regs(dev, REG_CALIB_H2, hbuf, 7))
    {
        return BME280_ERR_I2C;
    }

    dev->calib.dig_h2 = (int16_t)((uint16_t)hbuf[1] << 8 | hbuf[0]);
    dev->calib.dig_h3 = hbuf[2];
    dev->calib.dig_h4 = (int16_t)((int16_t)hbuf[3] << 4 | (hbuf[4] & 0x0F));
    dev->calib.dig_h5 = (int16_t)((int16_t)hbuf[5] << 4 | (hbuf[4] >> 4));
    dev->calib.dig_h6 = (int8_t)hbuf[6];

    return BME280_OK;
}

bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return BME280_ERR_PARAM;
    }

    if ((unsigned)cfg->osrs_t > 5 || (unsigned)cfg->osrs_p > 5
        || (unsigned)cfg->osrs_h > 5)
    {
        return BME280_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    uint8_t id = 0;
    if (!read_regs(dev, REG_ID, &id, 1))
    {
        return BME280_ERR_I2C;
    }
    if (id != BME280_CHIP_ID)
    {
        return BME280_ERR_ID;
    }

    bme280_err_t err = read_calibration(dev);
    if (err != BME280_OK)
    {
        return err;
    }

    /* Config register (standby + filter + spi3w) — defaults OK */
    uint8_t config = 0x00;
    if (!write_reg(dev, REG_CONFIG, config))
    {
        return BME280_ERR_I2C;
    }

    /* ctrl_hum must be written before ctrl_meas to take effect */
    uint8_t ctrl_hum = (uint8_t)(dev->cfg.osrs_h & 0x07);
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
    {
        return BME280_ERR_I2C;
    }

    return BME280_OK;
}

static int32_t compensate_temperature(bme280_t *dev, int32_t adc_t)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)dev->calib.dig_t1 << 1)))
                    * ((int32_t)dev->calib.dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)dev->calib.dig_t1))
                      * ((adc_t >> 4) - ((int32_t)dev->calib.dig_t1))) >> 12)
                    * ((int32_t)dev->calib.dig_t3)) >> 14;
    dev->t_fine = var1 + var2;
    return (dev->t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(const bme280_t *dev, int32_t adc_p)
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

static uint32_t compensate_humidity(const bme280_t *dev, int32_t adc_h)
{
    int32_t v = dev->t_fine - 76800;

    v = (((((adc_h << 14) - (((int32_t)dev->calib.dig_h4) << 20)
            - (((int32_t)dev->calib.dig_h5) * v)) + 16384) >> 15)
         * (((((((v * ((int32_t)dev->calib.dig_h6)) >> 10)
                * (((v * ((int32_t)dev->calib.dig_h3)) >> 11) + 32768)) >> 10)
              + 2097152) * ((int32_t)dev->calib.dig_h2) + 8192) >> 14));

    v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dev->calib.dig_h1)) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;

    return (uint32_t)(v >> 12);
}

bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out)
{
    if (!dev || !out)
    {
        return BME280_ERR_PARAM;
    }

    /* ctrl_hum must be written before ctrl_meas per datasheet */
    uint8_t ctrl_hum = (uint8_t)(dev->cfg.osrs_h & 0x07);
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
    {
        return BME280_ERR_I2C;
    }

    uint8_t ctrl = (uint8_t)(((dev->cfg.osrs_t & 0x07) << 5)
                             | ((dev->cfg.osrs_p & 0x07) << 2) | MODE_FORCED);
    if (!write_reg(dev, REG_CTRL_MEAS, ctrl))
    {
        return BME280_ERR_I2C;
    }

    /* Wait for measurement to complete */
    for (int tries = 0; tries < 500; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_STATUS, &status, 1))
        {
            return BME280_ERR_I2C;
        }
        if ((status & 0x08) == 0)
        {
            break;
        }
        if (tries == 499)
        {
            return BME280_ERR_TIMEOUT;
        }
    }

    /* Read all data: press(3) + temp(3) + hum(2) = 8 bytes from 0xF7 */
    uint8_t buf[8];
    if (!read_regs(dev, REG_PRESS_MSB, buf, 8))
    {
        return BME280_ERR_I2C;
    }

    int32_t adc_p = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_t = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    int32_t adc_h = ((int32_t)buf[6] << 8) | (int32_t)buf[7];

    /* Temperature must be compensated first to set t_fine */
    int32_t temp_raw = compensate_temperature(dev, adc_t);
    out->temperature_c = (float)temp_raw / 100.0f;

    uint32_t press_raw = compensate_pressure(dev, adc_p);
    out->pressure_pa = (float)press_raw;

    /* Humidity: result is Q22.10 fixed point (value / 1024 = %RH) */
    uint32_t hum_raw = compensate_humidity(dev, adc_h);
    out->humidity_pct = (float)hum_raw / 1024.0f;

    return BME280_OK;
}

bme280_err_t bme280_set_oversampling(bme280_t *dev,
                                     bme280_oversampling_t osrs_t,
                                     bme280_oversampling_t osrs_p,
                                     bme280_oversampling_t osrs_h)
{
    if (!dev)
    {
        return BME280_ERR_PARAM;
    }
    if ((unsigned)osrs_t > 5 || (unsigned)osrs_p > 5 || (unsigned)osrs_h > 5)
    {
        return BME280_ERR_PARAM;
    }

    dev->cfg.osrs_t = osrs_t;
    dev->cfg.osrs_p = osrs_p;
    dev->cfg.osrs_h = osrs_h;

    return BME280_OK;
}

bme280_err_t bme280_reset(bme280_t *dev)
{
    if (!dev)
    {
        return BME280_ERR_PARAM;
    }
    if (!write_reg(dev, REG_RESET, RESET_CMD))
    {
        return BME280_ERR_I2C;
    }
    return BME280_OK;
}
