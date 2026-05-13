// SPDX-License-Identifier: MIT
/*
 * bmi160 — Bosch BMI160 6-axis inertial measurement unit
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "bmi160.h"
#include "contract.h"
#include <string.h>

/* ---------- register map ---------- */

#define REG_CHIP_ID       0x00u
#define REG_PMU_STATUS    0x03u
#define REG_GYR_DATA_XL   0x0Cu
#define REG_ACC_DATA_XL   0x12u
#define REG_ACC_RANGE     0x41u
#define REG_GYR_RANGE     0x43u
#define REG_CMD           0x7Eu

/* CMD register commands */
#define CMD_SOFT_RESET    0xB6u
#define CMD_ACC_NORMAL    0x11u
#define CMD_GYR_NORMAL    0x15u

/* ---------- sensitivity tables ---------- */

/* Accel sensitivity: mg per LSB (16-bit signed output)
 * Indexed by a helper that maps range enum to table index. */
static const float accel_sens[] = {
    0.061f,    /* +/-2g  */
    0.122f,    /* +/-4g  */
    0.244f,    /* +/-8g  */
    0.488f,    /* +/-16g */
};

/* Gyro sensitivity: dps per LSB (16-bit signed output) */
static const float gyro_sens[] = {
    0.06104f,  /* +/-2000 dps */
    0.03052f,  /* +/-1000 dps */
    0.01526f,  /* +/-500 dps  */
    0.00763f,  /* +/-250 dps  */
    0.00381f,  /* +/-125 dps  */
};

/* ---------- helpers ---------- */

static int accel_range_to_idx(bmi160_accel_range_t r)
{
    switch (r) {
    case BMI160_ACCEL_2G:  return 0;
    case BMI160_ACCEL_4G:  return 1;
    case BMI160_ACCEL_8G:  return 2;
    case BMI160_ACCEL_16G: return 3;
    default:               return -1;
    }
}

static int gyro_range_to_idx(bmi160_gyro_range_t r)
{
    switch (r) {
    case BMI160_GYRO_2000DPS: return 0;
    case BMI160_GYRO_1000DPS: return 1;
    case BMI160_GYRO_500DPS:  return 2;
    case BMI160_GYRO_250DPS:  return 3;
    case BMI160_GYRO_125DPS:  return 4;
    default:                  return -1;
    }
}

static bool write_reg(const bmi160_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bmi160_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static void delay_ms(const bmi160_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms) {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* ---------- API ---------- */

bmi160_err_t bmi160_init(bmi160_t *dev, const bmi160_i2c_t *i2c,
                         const bmi160_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg) {
        return BMI160_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg) {
        return BMI160_ERR_PARAM;
    }

    int acc_idx = accel_range_to_idx(cfg->accel_range);
    int gyr_idx = gyro_range_to_idx(cfg->gyro_range);
    if (acc_idx < 0 || gyr_idx < 0) {
        return BMI160_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Read and verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1)) {
        return BMI160_ERR_I2C;
    }
    if (id != BMI160_CHIP_ID_VAL) {
        return BMI160_ERR_ID;
    }

    /* Set accelerometer to normal mode */
    if (!write_reg(dev, REG_CMD, CMD_ACC_NORMAL)) {
        return BMI160_ERR_I2C;
    }
    delay_ms(dev, 4);  /* wait for accel power-up */

    /* Set gyroscope to normal mode */
    if (!write_reg(dev, REG_CMD, CMD_GYR_NORMAL)) {
        return BMI160_ERR_I2C;
    }
    delay_ms(dev, 80);  /* wait for gyro power-up */

    /* Configure accelerometer range */
    if (!write_reg(dev, REG_ACC_RANGE, (uint8_t)cfg->accel_range)) {
        return BMI160_ERR_I2C;
    }

    /* Configure gyroscope range */
    if (!write_reg(dev, REG_GYR_RANGE, (uint8_t)cfg->gyro_range)) {
        return BMI160_ERR_I2C;
    }

    /* Store sensitivity for data conversion */
    dev->accel_sensitivity = accel_sens[acc_idx];
    dev->gyro_sensitivity  = gyro_sens[gyr_idx];

    INVARIANT(dev->i2c != NULL);
    return BMI160_OK;
}

bmi160_err_t bmi160_read(const bmi160_t *dev, bmi160_data_t *out)
{
    if (!dev || !dev->i2c || !out) {
        return BMI160_ERR_PARAM;
    }

    /* Gyro data: 0x0C..0x11 (6 bytes, little-endian) */
    uint8_t gbuf[6];
    if (!read_regs(dev, REG_GYR_DATA_XL, gbuf, 6)) {
        return BMI160_ERR_I2C;
    }

    /* Accel data: 0x12..0x17 (6 bytes, little-endian) */
    uint8_t abuf[6];
    if (!read_regs(dev, REG_ACC_DATA_XL, abuf, 6)) {
        return BMI160_ERR_I2C;
    }

    /* Parse accel: little-endian 16-bit signed */
    int16_t raw_ax = (int16_t)((uint16_t)abuf[1] << 8 | abuf[0]);
    int16_t raw_ay = (int16_t)((uint16_t)abuf[3] << 8 | abuf[2]);
    int16_t raw_az = (int16_t)((uint16_t)abuf[5] << 8 | abuf[4]);

    out->accel_mg[0] = (float)raw_ax * dev->accel_sensitivity;
    out->accel_mg[1] = (float)raw_ay * dev->accel_sensitivity;
    out->accel_mg[2] = (float)raw_az * dev->accel_sensitivity;

    /* Parse gyro: little-endian 16-bit signed */
    int16_t raw_gx = (int16_t)((uint16_t)gbuf[1] << 8 | gbuf[0]);
    int16_t raw_gy = (int16_t)((uint16_t)gbuf[3] << 8 | gbuf[2]);
    int16_t raw_gz = (int16_t)((uint16_t)gbuf[5] << 8 | gbuf[4]);

    out->gyro_dps[0] = (float)raw_gx * dev->gyro_sensitivity;
    out->gyro_dps[1] = (float)raw_gy * dev->gyro_sensitivity;
    out->gyro_dps[2] = (float)raw_gz * dev->gyro_sensitivity;

    /* NaN guard */
    ENSURE(out->accel_mg[0] == out->accel_mg[0]);
    ENSURE(out->accel_mg[1] == out->accel_mg[1]);
    ENSURE(out->accel_mg[2] == out->accel_mg[2]);
    ENSURE(out->gyro_dps[0] == out->gyro_dps[0]);
    ENSURE(out->gyro_dps[1] == out->gyro_dps[1]);
    ENSURE(out->gyro_dps[2] == out->gyro_dps[2]);
    return BMI160_OK;
}

bmi160_err_t bmi160_reset(bmi160_t *dev)
{
    if (!dev || !dev->i2c) {
        return BMI160_ERR_PARAM;
    }
    if (!write_reg(dev, REG_CMD, CMD_SOFT_RESET)) {
        return BMI160_ERR_I2C;
    }
    return BMI160_OK;
}
