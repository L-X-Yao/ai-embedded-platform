// SPDX-License-Identifier: MIT
/*
 * mpu6050 — InvenSense MPU-6050 6-axis IMU driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "mpu6050.h"
#include "contract.h"
#include <string.h>

#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B
#define REG_GYRO_XOUT_H   0x43
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

#define PWR_RESET         0x80
#define PWR_SLEEP         0x40
#define PWR_CLKSEL_PLL_X  0x01

/* DEVICE_RESET clears itself "once the reset is done" (register map p.42), with
 * no time given. The nearest figure is 100 ms at most for register access after
 * start-up (product specification p.14), which this many reads cover even at
 * 400 kHz with no delay_ms. */
#define RESET_POLL_TRIES  1000

static const float accel_scales[] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
static const float gyro_scales[]  = { 131.0f, 65.5f, 32.8f, 16.4f };

static bool write_reg(const mpu6050_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const mpu6050_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static mpu6050_err_t write_ranges(const mpu6050_t *dev)
{
    if (!write_reg(dev, REG_ACCEL_CONFIG, (uint8_t)(dev->cfg.accel_range << 3)))
    {
        return MPU6050_ERR_I2C;
    }
    if (!write_reg(dev, REG_GYRO_CONFIG, (uint8_t)(dev->cfg.gyro_range << 3)))
    {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_OK;
}

/* A failed read counts as not done: the manual does not say the chip answers
 * during a reset. */
static bool wait_reset_done(const mpu6050_t *dev)
{
    for (int tries = 0; tries < RESET_POLL_TRIES; tries++)
    {
        uint8_t pwr;
        if (read_regs(dev, REG_PWR_MGMT_1, &pwr, 1) && (pwr & PWR_RESET) == 0)
        {
            return true;
        }
        if (dev->i2c->delay_ms)
        {
            dev->i2c->delay_ms(dev->i2c->ctx, 1);
        }
    }
    return false;
}

/* DEVICE_RESET, then a wake-up once it is done. Linux waits 100 ms there and
 * Adafruit polls the bit. Asleep the chip reads zeros, which look like valid
 * data, so the wake-up is read back. */
static mpu6050_err_t reset_and_wake(const mpu6050_t *dev)
{
    if (!write_reg(dev, REG_PWR_MGMT_1, PWR_RESET))
    {
        return MPU6050_ERR_I2C;
    }
    if (!wait_reset_done(dev))
    {
        return MPU6050_ERR_TIMEOUT;
    }
    if (!write_reg(dev, REG_PWR_MGMT_1, PWR_CLKSEL_PLL_X))
    {
        return MPU6050_ERR_I2C;
    }

    uint8_t pwr = 0;
    if (!read_regs(dev, REG_PWR_MGMT_1, &pwr, 1) || (pwr & PWR_SLEEP) != 0)
    {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_OK;
}

/* The steps of init that talk to the chip */
static mpu6050_err_t bring_up(mpu6050_t *dev, const mpu6050_cfg_t *cfg)
{
    mpu6050_err_t err = reset_and_wake(dev);
    if (err != MPU6050_OK)
    {
        return err;
    }

    uint8_t id = 0;
    if (!read_regs(dev, REG_WHO_AM_I, &id, 1))
    {
        return MPU6050_ERR_I2C;
    }
    if (id != MPU6050_WHO_AM_I)
    {
        return MPU6050_ERR_ID;
    }

    if ((unsigned)cfg->accel_range > 3 || (unsigned)cfg->gyro_range > 3)
    {
        return MPU6050_ERR_PARAM;
    }
    err = write_ranges(dev);
    if (err != MPU6050_OK)
    {
        return err;
    }

    dev->accel_scale = accel_scales[cfg->accel_range & 0x03];
    dev->gyro_scale  = gyro_scales[cfg->gyro_range & 0x03];
    return MPU6050_OK;
}

mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           const mpu6050_cfg_t *cfg)
{
    if (!dev)
    {
        return MPU6050_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a device
     * without it, which has no scales and would read infinity */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
    {
        return MPU6050_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs)
    {
        return MPU6050_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;

    mpu6050_err_t err = bring_up(dev, cfg);
    if (err != MPU6050_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return MPU6050_OK;
}

mpu6050_err_t mpu6050_read_accel(const mpu6050_t *dev, mpu6050_accel_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return MPU6050_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_ACCEL_XOUT_H, buf, 6))
    {
        return MPU6050_ERR_I2C;
    }

    int16_t raw_x = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    int16_t raw_y = (int16_t)((uint16_t)buf[2] << 8 | buf[3]);
    int16_t raw_z = (int16_t)((uint16_t)buf[4] << 8 | buf[5]);

    out->ax_g = (float)raw_x / dev->accel_scale;
    out->ay_g = (float)raw_y / dev->accel_scale;
    out->az_g = (float)raw_z / dev->accel_scale;

    ENSURE(out->ax_g == out->ax_g);  /* not NaN */
    ENSURE(out->ay_g == out->ay_g);
    ENSURE(out->az_g == out->az_g);
    return MPU6050_OK;
}

mpu6050_err_t mpu6050_read_gyro(const mpu6050_t *dev, mpu6050_gyro_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return MPU6050_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_GYRO_XOUT_H, buf, 6))
    {
        return MPU6050_ERR_I2C;
    }

    int16_t raw_x = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    int16_t raw_y = (int16_t)((uint16_t)buf[2] << 8 | buf[3]);
    int16_t raw_z = (int16_t)((uint16_t)buf[4] << 8 | buf[5]);

    out->gx_dps = (float)raw_x / dev->gyro_scale;
    out->gy_dps = (float)raw_y / dev->gyro_scale;
    out->gz_dps = (float)raw_z / dev->gyro_scale;

    ENSURE(out->gx_dps == out->gx_dps);  /* not NaN */
    ENSURE(out->gy_dps == out->gy_dps);
    ENSURE(out->gz_dps == out->gz_dps);
    return MPU6050_OK;
}

mpu6050_err_t mpu6050_reset(mpu6050_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return MPU6050_ERR_PARAM;
    }

    /* The reset leaves register 107 at 0x40, asleep, and both ranges at 0
     * (register map p.8), while dev keeps its scales: put back what init set. */
    mpu6050_err_t err = reset_and_wake(dev);
    if (err != MPU6050_OK)
    {
        return err;
    }
    return write_ranges(dev);
}
