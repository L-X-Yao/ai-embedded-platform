// SPDX-License-Identifier: MIT
/*
 * qmi8658a — QMI8658A 6-axis IMU driver (I2C)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "qmi8658a.h"
#include "contract.h"

/* ---------- register map ---------- */

#define REG_WHO_AM_I   0x00
#define REG_CTRL1      0x02
#define REG_CTRL2      0x03
#define REG_CTRL3      0x04
#define REG_CTRL7      0x08
#define REG_RESET      0x60
#define REG_AX_L       0x35

#define WHO_AM_I_VAL   0x05
#define DEV_ADDR       0x6B

/* ---------- scale tables ---------- */

static const float acc_scale[] = {
    /* ±2g    ±4g     ±8g     ±16g */
    16384.0f, 8192.0f, 4096.0f, 2048.0f
};

static const float gyr_scale[] = {
    /* ±16   ±32    ±64    ±128   ±256   ±512   ±1024  ±2048 */
    2048.0f, 1024.0f, 512.0f, 256.0f, 128.0f, 64.0f, 32.0f, 16.0f
};

/* ---------- helpers ---------- */

static qmi8658a_err_t wr(const qmi8658a_i2c_t *i2c, uint8_t reg, uint8_t val)
{
    return i2c->write_reg(i2c->ctx, DEV_ADDR, reg, val)
           ? QMI8658A_OK : QMI8658A_ERR_I2C;
}

static qmi8658a_err_t rd(const qmi8658a_i2c_t *i2c, uint8_t reg,
                          uint8_t *buf, uint16_t len)
{
    return i2c->read_regs(i2c->ctx, DEV_ADDR, reg, buf, len)
           ? QMI8658A_OK : QMI8658A_ERR_I2C;
}

/* ---------- API ---------- */

qmi8658a_cfg_t qmi8658a_default_cfg(void)
{
    qmi8658a_cfg_t c;
    c.acc_range = QMI8658A_ACC_RANGE_8G;
    c.gyr_range = QMI8658A_GYR_RANGE_2048;
    c.acc_odr   = QMI8658A_ODR_500;
    c.gyr_odr   = QMI8658A_ODR_500;
    return c;
}

qmi8658a_err_t qmi8658a_init(qmi8658a_t *dev,
                              const qmi8658a_i2c_t *i2c,
                              const qmi8658a_cfg_t *cfg)
{
    /* ERR_I2C is reused for param errors — no dedicated ERR_PARAM code in this
     * driver's enum, and adding one would break API.  Callers see "init failed"
     * which is the correct recovery path regardless of root cause. */
    if (!dev || !i2c || !cfg) return QMI8658A_ERR_I2C;
    if ((unsigned)cfg->acc_range > 3 || (unsigned)cfg->gyr_range > 7
        || (unsigned)cfg->acc_odr > 8 || (unsigned)cfg->gyr_odr > 8)
        return QMI8658A_ERR_I2C;
    dev->i2c = i2c;
    uint8_t id = 0;
    qmi8658a_err_t e;

    e = rd(i2c, REG_WHO_AM_I, &id, 1);
    if (e != QMI8658A_OK) return e;
    if (id != WHO_AM_I_VAL) return QMI8658A_ERR_CHIP_ID;

    e = wr(i2c, REG_CTRL1, 0x40);
    if (e != QMI8658A_OK) return e;

    uint8_t ctrl2 = (uint8_t)(((uint8_t)cfg->acc_range << 4) | ((uint8_t)cfg->acc_odr));
    e = wr(i2c, REG_CTRL2, ctrl2);
    if (e != QMI8658A_OK) return e;

    uint8_t ctrl3 = (uint8_t)(((uint8_t)cfg->gyr_range << 4) | ((uint8_t)cfg->gyr_odr));
    e = wr(i2c, REG_CTRL3, ctrl3);
    if (e != QMI8658A_OK) return e;

    e = wr(i2c, REG_CTRL7, 0x03);
    if (e != QMI8658A_OK) return e;

    dev->acc_div = acc_scale[cfg->acc_range & 0x03];
    dev->gyr_div = gyr_scale[cfg->gyr_range & 0x07];

    INVARIANT(dev->i2c != NULL);
    return QMI8658A_OK;
}

qmi8658a_err_t qmi8658a_read(const qmi8658a_t *dev,
                              qmi8658a_data_t *out)
{
    if (!dev || !dev->i2c || !out) return QMI8658A_ERR_I2C;
    uint8_t raw[12];
    qmi8658a_err_t e = rd(dev->i2c, REG_AX_L, raw, 12);
    if (e != QMI8658A_OK) return e;

    /* Assemble big-endian pairs into signed 16-bit, then scale. */
    int16_t ax = (int16_t)((uint16_t)raw[1]  << 8 | raw[0]);
    int16_t ay = (int16_t)((uint16_t)raw[3]  << 8 | raw[2]);
    int16_t az = (int16_t)((uint16_t)raw[5]  << 8 | raw[4]);
    int16_t gx = (int16_t)((uint16_t)raw[7]  << 8 | raw[6]);
    int16_t gy = (int16_t)((uint16_t)raw[9]  << 8 | raw[8]);
    int16_t gz = (int16_t)((uint16_t)raw[11] << 8 | raw[10]);

    out->ax_g   = (float)ax / dev->acc_div;
    out->ay_g   = (float)ay / dev->acc_div;
    out->az_g   = (float)az / dev->acc_div;
    out->gx_dps = (float)gx / dev->gyr_div;
    out->gy_dps = (float)gy / dev->gyr_div;
    out->gz_dps = (float)gz / dev->gyr_div;

    ENSURE(out->ax_g == out->ax_g);      /* not NaN */
    ENSURE(out->ay_g == out->ay_g);
    ENSURE(out->az_g == out->az_g);
    ENSURE(out->gx_dps == out->gx_dps);
    ENSURE(out->gy_dps == out->gy_dps);
    ENSURE(out->gz_dps == out->gz_dps);
    return QMI8658A_OK;
}

qmi8658a_err_t qmi8658a_reset(const qmi8658a_t *dev)
{
    if (!dev || !dev->i2c) return QMI8658A_ERR_I2C;
    return wr(dev->i2c, REG_RESET, 0xB0);
}
