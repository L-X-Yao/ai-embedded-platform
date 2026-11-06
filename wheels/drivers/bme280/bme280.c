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
#include "contract.h"
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
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bme280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
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
    dev->calib.dig_h4 = (int16_t)(((int16_t)(int8_t)hbuf[3] * 16) | (int16_t)(hbuf[4] & 0x0F));
    dev->calib.dig_h5 = (int16_t)(((int16_t)(int8_t)hbuf[5] * 16) | (int16_t)(hbuf[4] >> 4));
    dev->calib.dig_h6 = (int8_t)hbuf[6];

    return BME280_OK;
}

/* Codes run 0..5, and all three SKIP would leave nothing to measure */
static bool osrs_valid(bme280_oversampling_t osrs_t, bme280_oversampling_t osrs_p,
                       bme280_oversampling_t osrs_h)
{
    if ((unsigned)osrs_t > 5 || (unsigned)osrs_p > 5 || (unsigned)osrs_h > 5)
    {
        return false;
    }
    return osrs_t != BME280_OVERSAMPLING_SKIP || osrs_p != BME280_OVERSAMPLING_SKIP
           || osrs_h != BME280_OVERSAMPLING_SKIP;
}

/* Sets t_fine, which pressure and humidity compensation use */
static void compensate_temperature(bme280_t *dev, int32_t adc_t)
{
    int32_t var1 = (int32_t)(((int64_t)(adc_t >> 3) - ((int64_t)dev->calib.dig_t1 * 2))
                    * (int64_t)dev->calib.dig_t2 >> 11);
    int64_t tmp = (int64_t)(adc_t >> 4) - (int64_t)dev->calib.dig_t1;
    int32_t var2 = (int32_t)(((tmp * tmp) >> 12) * (int64_t)dev->calib.dig_t3 >> 14);
    dev->t_fine = var1 + var2;
}

/* adc_t and adc_p: 20 bits, MSB first, xlsb in bits 7:4 */
static int32_t adc_20bit(const uint8_t *buf)
{
    return (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
}

/*
 * One forced measurement, leaving the 8 data bytes from 0xF7 in buf. A skipped
 * channel holds 0x80000, or 0x8000 for humidity (manual Tables 20, 23, 24). That
 * is a placeholder, not a measurement, so it must not be compensated.
 */
static bme280_err_t measure(const bme280_t *dev, bme280_oversampling_t osrs_t,
                            bme280_oversampling_t osrs_p,
                            bme280_oversampling_t osrs_h, uint8_t *buf)
{
    /* ctrl_hum must be written before ctrl_meas per datasheet */
    uint8_t ctrl_hum = (uint8_t)(osrs_h & 0x07);
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
    {
        return BME280_ERR_I2C;
    }

    uint8_t ctrl = (uint8_t)(((osrs_t & 0x07) << 5)
                             | ((osrs_p & 0x07) << 2) | MODE_FORCED);
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
    if (!read_regs(dev, REG_PRESS_MSB, buf, 8))
    {
        return BME280_ERR_I2C;
    }
    return BME280_OK;
}

/* Temperature alone at x1, for reads that skip it and so cannot set t_fine */
static bme280_err_t measure_temperature(bme280_t *dev)
{
    uint8_t buf[8];
    bme280_err_t err = measure(dev, BME280_OVERSAMPLING_1, BME280_OVERSAMPLING_SKIP,
                               BME280_OVERSAMPLING_SKIP, buf);
    if (err != BME280_OK)
    {
        return err;
    }
    compensate_temperature(dev, adc_20bit(&buf[3]));
    return BME280_OK;
}

bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return BME280_ERR_PARAM;
    }

    if (!osrs_valid(cfg->osrs_t, cfg->osrs_p, cfg->osrs_h))
    {
        return BME280_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
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

    /* Pressure and humidity compensation need t_fine, and reads that skip
     * temperature cannot set it, so measure it once here for them to reuse. */
    if (cfg->osrs_t == BME280_OVERSAMPLING_SKIP)
    {
        err = measure_temperature(dev);
        if (err != BME280_OK)
        {
            return err;
        }
    }

    /* ctrl_hum must be written before ctrl_meas to take effect */
    uint8_t ctrl_hum = (uint8_t)(dev->cfg.osrs_h & 0x07);
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
    {
        return BME280_ERR_I2C;
    }

    return BME280_OK;
}

static double compensate_pressure_f(const bme280_t *dev, int32_t adc_p)
{
    double var1 = ((double)dev->t_fine / 2.0) - 64000.0;
    double var2 = var1 * var1 * (double)dev->calib.dig_p6 / 32768.0;
    var2 = var2 + var1 * (double)dev->calib.dig_p5 * 2.0;
    var2 = var2 / 4.0 + (double)dev->calib.dig_p4 * 65536.0;
    var1 = ((double)dev->calib.dig_p3 * var1 * var1 / 524288.0
            + (double)dev->calib.dig_p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)dev->calib.dig_p1;
    if (var1 == 0.0) return 0.0;
    double p = 1048576.0 - (double)adc_p;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = (double)dev->calib.dig_p9 * p * p / 2147483648.0;
    var2 = p * (double)dev->calib.dig_p8 / 32768.0;
    return p + (var1 + var2 + (double)dev->calib.dig_p7) / 16.0;
}

static uint32_t compensate_humidity(const bme280_t *dev, int32_t adc_h)
{
    int64_t v1 = (int64_t)dev->t_fine - 76800;

    int64_t v2 = (int64_t)adc_h * 16384 - (int64_t)dev->calib.dig_h4 * 1048576
                 - (int64_t)dev->calib.dig_h5 * v1;
    v2 = (v2 + 16384) >> 15;

    int64_t v3 = ((v1 * (int64_t)dev->calib.dig_h6) >> 10)
                 * (((v1 * (int64_t)dev->calib.dig_h3) >> 11) + 32768);
    v3 = ((v3 >> 10) + 2097152) * (int64_t)dev->calib.dig_h2 + 8192;
    v3 >>= 14;

    int64_t v4 = v2 * v3;
    v4 -= ((((v4 >> 15) * (v4 >> 15)) >> 7) * (int64_t)dev->calib.dig_h1) >> 4;
    if (v4 < 0) v4 = 0;
    if (v4 > 419430400) v4 = 419430400;

    return (uint32_t)(v4 >> 12);
}

bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out)
{
    if (!dev || !out)
    {
        return BME280_ERR_PARAM;
    }

    bool temp  = dev->cfg.osrs_t != BME280_OVERSAMPLING_SKIP;
    bool press = dev->cfg.osrs_p != BME280_OVERSAMPLING_SKIP;
    bool hum   = dev->cfg.osrs_h != BME280_OVERSAMPLING_SKIP;
    bme280_err_t err;

    /* bme280_set_oversampling can skip temperature before anything measured
     * it. t_fine 0 is also a real 0.00 C, which only costs a second measurement. */
    if (!temp && dev->t_fine == 0)
    {
        err = measure_temperature(dev);
        if (err != BME280_OK)
        {
            return err;
        }
    }

    uint8_t buf[8];
    err = measure(dev, dev->cfg.osrs_t, dev->cfg.osrs_p, dev->cfg.osrs_h, buf);
    if (err != BME280_OK)
    {
        return err;
    }

    /* Temperature must be compensated first to set t_fine. When it is skipped,
     * t_fine still holds the last one measured. */
    if (temp)
    {
        compensate_temperature(dev, adc_20bit(&buf[3]));
    }
    int32_t temp_raw = (dev->t_fine * 5 + 128) >> 8;
    out->temperature_c = (float)temp_raw / 100.0f;

    out->pressure_pa = press ? (float)compensate_pressure_f(dev, adc_20bit(&buf[0])) : 0.0f;

    /* Humidity: result is Q22.10 fixed point (value / 1024 = %RH) */
    int32_t adc_h = (int32_t)(((uint32_t)buf[6] << 8) | (uint32_t)buf[7]);
    out->humidity_pct = hum ? (float)compensate_humidity(dev, adc_h) / 1024.0f : 0.0f;

    INVARIANT(dev->i2c != NULL);
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
    if (!osrs_valid(osrs_t, osrs_p, osrs_h))
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
