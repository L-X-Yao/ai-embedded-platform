// SPDX-License-Identifier: MIT
/*
 * prop_mpu6050.c — property-based tests for MPU-6050 IMU driver
 *
 * Properties:
 *   1. init+read never crashes with arbitrary mock register values
 *   2. accel output finite when init succeeds
 *   3. gyro output finite when init succeeds
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "mpu6050.h"

/* DEVICE_RESET clears itself, which the plain register mock would not do */
static bool prop_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    if (reg == 0x6B && (val & 0x80))
        val = 0x40;
    return mock_i2c_write_reg(ctx, addr, reg, val);
}

static const mpu6050_i2c_t prop_i2c = {
    .write_reg = prop_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = MPU6050_WHO_AM_I; /* WHO_AM_I register */

    mpu6050_t dev;
    mpu6050_cfg_t cfg = {
        .addr        = MPU6050_ADDR_LOW,
        .accel_range = MPU6050_ACCEL_2G,
        .gyro_range  = MPU6050_GYRO_250DPS,
    };
    mpu6050_init(&dev, &prop_i2c, &cfg);

    mpu6050_accel_t accel;
    mpu6050_read_accel(&dev, &accel);
    mpu6050_gyro_t gyro;
    mpu6050_read_gyro(&dev, &gyro);
    /* must not crash */
}

PROPERTY(accel_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = MPU6050_WHO_AM_I;

    mpu6050_t dev;
    mpu6050_cfg_t cfg = {
        .addr        = MPU6050_ADDR_LOW,
        .accel_range = (mpu6050_accel_range_t)(ARBI_UINT8(seed) % 4),
        .gyro_range  = (mpu6050_gyro_range_t)(ARBI_UINT8(seed) % 4),
    };
    mpu6050_err_t err = mpu6050_init(&dev, &prop_i2c, &cfg);
    if (err != MPU6050_OK) return;

    mpu6050_accel_t accel;
    err = mpu6050_read_accel(&dev, &accel);
    if (err != MPU6050_OK) return;

    PROP_ASSERT(accel.ax_g == accel.ax_g); /* not NaN */
    PROP_ASSERT(accel.ay_g == accel.ay_g);
    PROP_ASSERT(accel.az_g == accel.az_g);
}

PROPERTY(gyro_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = MPU6050_WHO_AM_I;

    mpu6050_t dev;
    mpu6050_cfg_t cfg = {
        .addr        = MPU6050_ADDR_LOW,
        .accel_range = MPU6050_ACCEL_2G,
        .gyro_range  = (mpu6050_gyro_range_t)(ARBI_UINT8(seed) % 4),
    };
    mpu6050_err_t err = mpu6050_init(&dev, &prop_i2c, &cfg);
    if (err != MPU6050_OK) return;

    mpu6050_gyro_t gyro;
    err = mpu6050_read_gyro(&dev, &gyro);
    if (err != MPU6050_OK) return;

    PROP_ASSERT(gyro.gx_dps == gyro.gx_dps);
    PROP_ASSERT(gyro.gy_dps == gyro.gy_dps);
    PROP_ASSERT(gyro.gz_dps == gyro.gz_dps);
}

TEST_MAIN("prop_mpu6050")
    RUN(read_no_crash);
    RUN(accel_finite);
    RUN(gyro_finite);
TEST_MAIN_END()
