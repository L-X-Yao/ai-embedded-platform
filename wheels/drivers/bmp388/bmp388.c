// SPDX-License-Identifier: MIT
/*
 * bmp388 — Bosch BMP388 barometric pressure and temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithm from Bosch BMP388 datasheet (BST-BMP388-DS001),
 * Section 9.3 (floating point).
 */

#include "bmp388.h"
#include "contract.h"
#include <string.h>
#include <math.h>

#define REG_CHIP_ID    0x00
#define REG_STATUS     0x03
#define REG_DATA_0     0x04  /* press XLSB */
#define REG_CALIB_00   0x31  /* NVM calibration start */
#define REG_PWR_CTRL   0x1B
#define REG_OSR        0x1C
#define REG_CMD        0x7E

#define RESET_CMD      0xB6
#define CALIB_LEN      21

/* PWR_CTRL bits: press_en=bit0, temp_en=bit1, mode=bits[5:4] */
#define PWR_CTRL_FORCED  0x13  /* press_en | temp_en | mode=forced(01) */

/* STATUS bit3 = drdy_press, bit5 = drdy_temp */
#define STATUS_DRDY_PRESS  0x20
#define STATUS_DRDY_TEMP   0x40

static bool write_reg(const bmp388_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bmp388_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

/*
 * Parse NVM calibration bytes into float coefficients.
 * Raw NVM layout (21 bytes at 0x31-0x45):
 *   [0:1]   NVM_PAR_T1   (uint16_t LE)
 *   [2:3]   NVM_PAR_T2   (uint16_t LE)
 *   [4]     NVM_PAR_T3   (int8_t)
 *   [5:6]   NVM_PAR_P1   (int16_t LE)
 *   [7:8]   NVM_PAR_P2   (int16_t LE)
 *   [9]     NVM_PAR_P3   (int8_t)
 *   [10]    NVM_PAR_P4   (int8_t)
 *   [11:12] NVM_PAR_P5   (uint16_t LE)
 *   [13:14] NVM_PAR_P6   (uint16_t LE)
 *   [15]    NVM_PAR_P7   (int8_t)
 *   [16]    NVM_PAR_P8   (int8_t)
 *   [17:18] NVM_PAR_P9   (int16_t LE)
 *   [19]    NVM_PAR_P10  (int8_t)
 *   [20]    NVM_PAR_P11  (int8_t)
 *
 * Conversion to float as per datasheet Section 9.1.
 */
static bmp388_err_t read_calibration(bmp388_t *dev)
{
    uint8_t buf[CALIB_LEN];
    if (!read_regs(dev, REG_CALIB_00, buf, CALIB_LEN))
    {
        return BMP388_ERR_I2C;
    }

    uint16_t nvm_t1 = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    uint16_t nvm_t2 = (uint16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int8_t   nvm_t3 = (int8_t)buf[4];

    int16_t  nvm_p1  = (int16_t)((uint16_t)buf[6]  << 8 | buf[5]);
    int16_t  nvm_p2  = (int16_t)((uint16_t)buf[8]  << 8 | buf[7]);
    int8_t   nvm_p3  = (int8_t)buf[9];
    int8_t   nvm_p4  = (int8_t)buf[10];
    uint16_t nvm_p5  = (uint16_t)((uint16_t)buf[12] << 8 | buf[11]);
    uint16_t nvm_p6  = (uint16_t)((uint16_t)buf[14] << 8 | buf[13]);
    int8_t   nvm_p7  = (int8_t)buf[15];
    int8_t   nvm_p8  = (int8_t)buf[16];
    int16_t  nvm_p9  = (int16_t)((uint16_t)buf[18] << 8 | buf[17]);
    int8_t   nvm_p10 = (int8_t)buf[19];
    int8_t   nvm_p11 = (int8_t)buf[20];

    /* Datasheet Section 9.1: convert NVM values to float coefficients */
    dev->calib.par_t1  = (float)nvm_t1 / powf(2.0f, -8.0f);   /* * 2^8 */
    dev->calib.par_t2  = (float)nvm_t2 / powf(2.0f, 30.0f);   /* / 2^30 */
    dev->calib.par_t3  = (float)nvm_t3 / powf(2.0f, 48.0f);   /* / 2^48 */

    dev->calib.par_p1  = ((float)nvm_p1  - powf(2.0f, 14.0f)) / powf(2.0f, 20.0f);
    dev->calib.par_p2  = ((float)nvm_p2  - powf(2.0f, 14.0f)) / powf(2.0f, 29.0f);
    dev->calib.par_p3  = (float)nvm_p3  / powf(2.0f, 32.0f);
    dev->calib.par_p4  = (float)nvm_p4  / powf(2.0f, 37.0f);
    dev->calib.par_p5  = (float)nvm_p5  / powf(2.0f, -3.0f);  /* * 2^3 */
    dev->calib.par_p6  = (float)nvm_p6  / powf(2.0f, 6.0f);
    dev->calib.par_p7  = (float)nvm_p7  / powf(2.0f, 8.0f);
    dev->calib.par_p8  = (float)nvm_p8  / powf(2.0f, 15.0f);
    dev->calib.par_p9  = (float)nvm_p9  / powf(2.0f, 48.0f);
    dev->calib.par_p10 = (float)nvm_p10 / powf(2.0f, 48.0f);
    dev->calib.par_p11 = (float)nvm_p11 / powf(2.0f, 65.0f);

    return BMP388_OK;
}

/*
 * Temperature compensation (datasheet Section 9.3).
 * Returns compensated temperature in degrees C.
 * Also stores t_lin in dev for use by pressure compensation.
 */
static float compensate_temperature(bmp388_t *dev, uint32_t raw_temp)
{
    float partial1 = (float)raw_temp - dev->calib.par_t1;
    float partial2 = partial1 * dev->calib.par_t2;

    dev->t_lin = partial2 + (partial1 * partial1) * dev->calib.par_t3;
    return dev->t_lin;
}

/*
 * Pressure compensation (datasheet Section 9.3).
 * Returns compensated pressure in Pa.
 * Must be called after compensate_temperature (uses dev->t_lin).
 */
static float compensate_pressure(const bmp388_t *dev, uint32_t raw_press)
{
    float t = dev->t_lin;
    float t2 = t * t;
    float t3 = t2 * t;

    float pd1 = dev->calib.par_p6 * t;
    float pd2 = dev->calib.par_p7 * t2;
    float pd3 = dev->calib.par_p8 * t3;
    float out1 = dev->calib.par_p5 + pd1 + pd2 + pd3;

    float pd4 = dev->calib.par_p2 * t;
    float pd5 = dev->calib.par_p3 * t2;
    float pd6 = dev->calib.par_p4 * t3;
    float out2 = (float)raw_press * (dev->calib.par_p1 + pd4 + pd5 + pd6);

    float rp2 = (float)raw_press * (float)raw_press;
    float pd7 = dev->calib.par_p9 + dev->calib.par_p10 * t;
    float out3 = rp2 * pd7 + (float)raw_press * rp2 * dev->calib.par_p11;

    return out1 + out2 + out3;
}

bmp388_err_t bmp388_init(bmp388_t *dev, const bmp388_i2c_t *i2c,
                         const bmp388_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return BMP388_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs)
    {
        return BMP388_ERR_PARAM;
    }
    if ((unsigned)cfg->osrs_t > 6 || (unsigned)cfg->osrs_p > 6)
    {
        return BMP388_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
    {
        return BMP388_ERR_I2C;
    }
    if (id != BMP388_CHIP_ID)
    {
        return BMP388_ERR_ID;
    }

    /* Read calibration NVM */
    bmp388_err_t err = read_calibration(dev);
    if (err != BMP388_OK)
    {
        return err;
    }

    /* Configure oversampling: OSR register 0x1C
     * bits [2:0] = osr_p, bits [5:3] = osr_t */
    uint8_t osr = (uint8_t)(((cfg->osrs_t & 0x07) << 3) | (cfg->osrs_p & 0x07));
    if (!write_reg(dev, REG_OSR, osr))
    {
        return BMP388_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return BMP388_OK;
}

bmp388_err_t bmp388_read(bmp388_t *dev, bmp388_data_t *out)
{
    if (!dev || !out)
    {
        return BMP388_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return BMP388_ERR_PARAM;
    }

    /* Trigger forced measurement: PWR_CTRL = press_en | temp_en | mode=forced */
    if (!write_reg(dev, REG_PWR_CTRL, PWR_CTRL_FORCED))
    {
        return BMP388_ERR_I2C;
    }

    /* Wait for data ready (STATUS bit5=drdy_temp, bit6 is not used;
     * we check bit5 for temp ready which also implies press ready) */
    for (int tries = 0; tries < 500; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_STATUS, &status, 1))
        {
            return BMP388_ERR_I2C;
        }
        if (status & STATUS_DRDY_PRESS)
        {
            break;
        }
        if (tries == 499)
        {
            return BMP388_ERR_TIMEOUT;
        }
    }

    /* Read 6 bytes: press XLSB/LSB/MSB, temp XLSB/LSB/MSB */
    uint8_t buf[6];
    if (!read_regs(dev, REG_DATA_0, buf, 6))
    {
        return BMP388_ERR_I2C;
    }

    uint32_t raw_press = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
    uint32_t raw_temp  = (uint32_t)buf[3] | ((uint32_t)buf[4] << 8) | ((uint32_t)buf[5] << 16);

    /* Compensate temperature first (sets dev->t_lin) */
    out->temperature_c = compensate_temperature(dev, raw_temp);

    /* Compensate pressure (uses dev->t_lin) */
    out->pressure_pa = compensate_pressure(dev, raw_press);

    /* NaN guard */
    ENSURE(out->temperature_c == out->temperature_c);
    ENSURE(out->pressure_pa == out->pressure_pa);
    ENSURE(out->temperature_c >= -40.0f && out->temperature_c <= 85.0f);
    ENSURE(out->pressure_pa >= 30000.0f && out->pressure_pa <= 125000.0f);
    return BMP388_OK;
}

bmp388_err_t bmp388_reset(bmp388_t *dev)
{
    if (!dev)
    {
        return BMP388_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return BMP388_ERR_PARAM;
    }
    if (!write_reg(dev, REG_CMD, RESET_CMD))
    {
        return BMP388_ERR_I2C;
    }
    return BMP388_OK;
}
