/*
 * mpu6050 — InvenSense MPU-6050 6-axis IMU driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "mpu6050.h"
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
#define PWR_CLKSEL_PLL_X  0x01

static const float accel_scales[] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
static const float gyro_scales[]  = { 131.0f, 65.5f, 32.8f, 16.4f };

static bool write_reg(const mpu6050_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c.write_reg(dev->i2c.ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const mpu6050_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return dev->i2c.read_regs(dev->i2c.ctx, dev->cfg.addr, reg, buf, len);
}

mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           const mpu6050_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return MPU6050_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    if (!write_reg(dev, REG_PWR_MGMT_1, PWR_RESET))
    {
        return MPU6050_ERR_I2C;
    }

    if (!write_reg(dev, REG_PWR_MGMT_1, PWR_CLKSEL_PLL_X))
    {
        return MPU6050_ERR_I2C;
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

    if (!write_reg(dev, REG_ACCEL_CONFIG, (uint8_t)(cfg->accel_range << 3)))
    {
        return MPU6050_ERR_I2C;
    }
    if (!write_reg(dev, REG_GYRO_CONFIG, (uint8_t)(cfg->gyro_range << 3)))
    {
        return MPU6050_ERR_I2C;
    }

    dev->accel_scale = accel_scales[cfg->accel_range];
    dev->gyro_scale  = gyro_scales[cfg->gyro_range];

    return MPU6050_OK;
}

mpu6050_err_t mpu6050_read_accel(mpu6050_t *dev, mpu6050_accel_t *out)
{
    if (!dev || !out)
    {
        return MPU6050_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_ACCEL_XOUT_H, buf, 6))
    {
        return MPU6050_ERR_I2C;
    }

    int16_t raw_x = (int16_t)(buf[0] << 8 | buf[1]);
    int16_t raw_y = (int16_t)(buf[2] << 8 | buf[3]);
    int16_t raw_z = (int16_t)(buf[4] << 8 | buf[5]);

    out->ax = (float)raw_x / dev->accel_scale;
    out->ay = (float)raw_y / dev->accel_scale;
    out->az = (float)raw_z / dev->accel_scale;

    return MPU6050_OK;
}

mpu6050_err_t mpu6050_read_gyro(mpu6050_t *dev, mpu6050_gyro_t *out)
{
    if (!dev || !out)
    {
        return MPU6050_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_GYRO_XOUT_H, buf, 6))
    {
        return MPU6050_ERR_I2C;
    }

    int16_t raw_x = (int16_t)(buf[0] << 8 | buf[1]);
    int16_t raw_y = (int16_t)(buf[2] << 8 | buf[3]);
    int16_t raw_z = (int16_t)(buf[4] << 8 | buf[5]);

    out->gx = (float)raw_x / dev->gyro_scale;
    out->gy = (float)raw_y / dev->gyro_scale;
    out->gz = (float)raw_z / dev->gyro_scale;

    return MPU6050_OK;
}

mpu6050_err_t mpu6050_reset(mpu6050_t *dev)
{
    if (!dev)
    {
        return MPU6050_ERR_PARAM;
    }
    if (!write_reg(dev, REG_PWR_MGMT_1, PWR_RESET))
    {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_OK;
}
