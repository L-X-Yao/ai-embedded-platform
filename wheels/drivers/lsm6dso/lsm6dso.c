// SPDX-License-Identifier: MIT
/*
 * lsm6dso — ST LSM6DSO 6-axis accelerometer + gyroscope driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "lsm6dso.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_WHO_AM_I    0x0F
#define REG_CTRL1_XL    0x10    /* accel ODR + FS */
#define REG_CTRL2_G     0x11    /* gyro ODR + FS */
#define REG_CTRL3_C     0x12    /* control: SW_RESET (bit 0), BDU (bit 6) */
#define REG_STATUS      0x1E
#define REG_OUT_TEMP_L  0x20
#define REG_OUTX_L_G   0x22    /* gyro data start (6 bytes, little-endian) */
#define REG_OUTX_L_A   0x28    /* accel data start (6 bytes, little-endian) */

#define CTRL3_C_SW_RESET  0x01
#define CTRL3_C_IF_INC    0x04
#define CTRL3_C_BDU       0x40

/* ---------- sensitivity tables ---------- */

/*
 * Accel sensitivity in mg/LSB indexed by FS_XL[1:0] bits in CTRL1_XL[3:2].
 * Range enum values: 2G=0, 16G=1, 4G=2, 8G=3
 */
static float accel_sensitivity_from_range(lsm6dso_accel_range_t range)
{
    switch (range) {
    case LSM6DSO_ACCEL_2G:  return 0.061f;
    case LSM6DSO_ACCEL_4G:  return 0.122f;
    case LSM6DSO_ACCEL_8G:  return 0.244f;
    case LSM6DSO_ACCEL_16G: return 0.488f;
    default:                return 0.0f;
    }
}

/*
 * Gyro sensitivity in mdps/LSB indexed by FS_G[3:0] bits in CTRL2_G[3:0].
 * Range enum values: 125=2, 250=0, 500=4, 1000=8, 2000=12
 * Stored as mdps/LSB; driver divides by 1000 to produce dps output.
 */
static float gyro_sensitivity_from_range(lsm6dso_gyro_range_t range)
{
    switch (range) {
    case LSM6DSO_GYRO_125DPS:  return 4.375f;
    case LSM6DSO_GYRO_250DPS:  return 8.750f;
    case LSM6DSO_GYRO_500DPS:  return 17.500f;
    case LSM6DSO_GYRO_1000DPS: return 35.000f;
    case LSM6DSO_GYRO_2000DPS: return 70.000f;
    default:                   return 0.0f;
    }
}

/* ---------- internal helpers ---------- */

static bool write_reg(const lsm6dso_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const lsm6dso_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static lsm6dso_err_t bring_up(lsm6dso_t *dev, const lsm6dso_cfg_t *cfg)
{
    /* SW_RESET */
    if (!write_reg(dev, REG_CTRL3_C, CTRL3_C_SW_RESET))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Verify WHO_AM_I */
    uint8_t id = 0;
    if (!read_regs(dev, REG_WHO_AM_I, &id, 1))
    {
        return LSM6DSO_ERR_I2C;
    }
    if (id != LSM6DSO_WHO_AM_I)
    {
        return LSM6DSO_ERR_ID;
    }

    /* Compute sensitivities */
    dev->accel_sensitivity = accel_sensitivity_from_range(cfg->accel_range);
    dev->gyro_sensitivity  = gyro_sensitivity_from_range(cfg->gyro_range);

    if (dev->accel_sensitivity == 0.0f || dev->gyro_sensitivity == 0.0f)
    {
        return LSM6DSO_ERR_PARAM;
    }

    /* Configure CTRL1_XL: ODR[7:4] | FS_XL[3:2] */
    uint8_t ctrl1 = (uint8_t)((uint8_t)(cfg->odr << 4) |
                               (uint8_t)(cfg->accel_range << 2));
    if (!write_reg(dev, REG_CTRL1_XL, ctrl1))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Configure CTRL2_G: ODR[7:4] | FS_G[3:0]
     * Enum values already encode the correct bit pattern for CTRL2_G[3:0]:
     *   125dps  = 0b0010 (FS_125 flag in bit 1)
     *   250dps  = 0b0000
     *   500dps  = 0b0100
     *   1000dps = 0b1000
     *   2000dps = 0b1100
     * No additional shift needed. */
    uint8_t ctrl2 = (uint8_t)((uint8_t)(cfg->odr << 4) |
                               (uint8_t)(cfg->gyro_range));
    if (!write_reg(dev, REG_CTRL2_G, ctrl2))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Enable BDU in CTRL3_C for data consistency, and keep IF_INC set so that
     * multi-byte reads advance */
    if (!write_reg(dev, REG_CTRL3_C, CTRL3_C_BDU | CTRL3_C_IF_INC))
    {
        return LSM6DSO_ERR_I2C;
    }

    return LSM6DSO_OK;
}

lsm6dso_err_t lsm6dso_init(lsm6dso_t *dev, const lsm6dso_i2c_t *i2c,
                            const lsm6dso_cfg_t *cfg)
{
    if (!dev)
    {
        return LSM6DSO_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it, which has no sensitivities and would read zeros */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
    {
        return LSM6DSO_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg)
    {
        return LSM6DSO_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;

    lsm6dso_err_t err = bring_up(dev, cfg);
    if (err != LSM6DSO_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    INVARIANT(dev->accel_sensitivity > 0.0f);
    INVARIANT(dev->gyro_sensitivity > 0.0f);
    return LSM6DSO_OK;
}

lsm6dso_err_t lsm6dso_read_accel(const lsm6dso_t *dev,
                                  lsm6dso_accel_data_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return LSM6DSO_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_OUTX_L_A, buf, 6))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Little-endian: low byte first */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->x_mg = (float)raw_x * dev->accel_sensitivity;
    out->y_mg = (float)raw_y * dev->accel_sensitivity;
    out->z_mg = (float)raw_z * dev->accel_sensitivity;

    ENSURE(out->x_mg == out->x_mg);  /* not NaN */
    ENSURE(out->y_mg == out->y_mg);
    ENSURE(out->z_mg == out->z_mg);
    return LSM6DSO_OK;
}

lsm6dso_err_t lsm6dso_read_gyro(const lsm6dso_t *dev,
                                 lsm6dso_gyro_data_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return LSM6DSO_ERR_PARAM;
    }

    uint8_t buf[6];
    if (!read_regs(dev, REG_OUTX_L_G, buf, 6))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Little-endian: low byte first */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    /* Sensitivity is in mdps/LSB; divide by 1000 for dps output */
    out->x_dps = (float)raw_x * dev->gyro_sensitivity / 1000.0f;
    out->y_dps = (float)raw_y * dev->gyro_sensitivity / 1000.0f;
    out->z_dps = (float)raw_z * dev->gyro_sensitivity / 1000.0f;

    ENSURE(out->x_dps == out->x_dps);  /* not NaN */
    ENSURE(out->y_dps == out->y_dps);
    ENSURE(out->z_dps == out->z_dps);
    return LSM6DSO_OK;
}

lsm6dso_err_t lsm6dso_read_temp(const lsm6dso_t *dev, float *temperature_c)
{
    if (!dev || !dev->i2c || !temperature_c)
    {
        return LSM6DSO_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_OUT_TEMP_L, buf, 2))
    {
        return LSM6DSO_ERR_I2C;
    }

    /* Little-endian int16, sensitivity 256 LSB/C, offset 25C */
    int16_t raw = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    *temperature_c = (float)raw / 256.0f + 25.0f;

    ENSURE(*temperature_c == *temperature_c);  /* not NaN */
    return LSM6DSO_OK;
}

lsm6dso_err_t lsm6dso_reset(lsm6dso_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return LSM6DSO_ERR_PARAM;
    }
    if (!write_reg(dev, REG_CTRL3_C, CTRL3_C_SW_RESET))
    {
        return LSM6DSO_ERR_I2C;
    }
    return LSM6DSO_OK;
}
