// SPDX-License-Identifier: MIT
/*
 * as5600 — ams AS5600 I2C 12-bit magnetic rotary position sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "as5600.h"

/* Register addresses */
#define REG_ZPOS_H    0x01u  /* start position high byte */
#define REG_ZPOS_L    0x02u  /* start position low byte  */
#define REG_MANG_H    0x05u  /* max angle high byte      */
#define REG_MANG_L    0x06u  /* max angle low byte       */
#define REG_CONF_H    0x07u  /* configuration high byte  */
#define REG_CONF_L    0x08u  /* configuration low byte   */
#define REG_STATUS    0x0Bu  /* magnet status             */
#define REG_RAW_ANG_H 0x0Cu  /* raw angle high byte      */
#define REG_RAW_ANG_L 0x0Du  /* raw angle low byte       */

#define ANGLE_MASK    0x0FFFu  /* 12-bit mask */
#define FULL_TURN_DEG 360.0f
#define FULL_TURN_CNT 4096.0f

static bool write_reg(const as5600_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c.write_reg(dev->i2c.ctx, AS5600_ADDR, reg, val);
}

static bool read_regs(const as5600_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c.read_regs(dev->i2c.ctx, AS5600_ADDR, reg, buf, len);
}

as5600_err_t as5600_init(as5600_t *dev, const as5600_i2c_t *i2c)
{
    if (!dev || !i2c)
        return AS5600_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return AS5600_ERR_PARAM;

    dev->i2c = *i2c;

    /* Verify communication by reading status register */
    uint8_t status = 0;
    if (!read_regs(dev, REG_STATUS, &status, 1))
        return AS5600_ERR_I2C;

    return AS5600_OK;
}

as5600_err_t as5600_read_raw_angle(const as5600_t *dev, uint16_t *raw)
{
    if (!dev || !raw)
        return AS5600_ERR_PARAM;

    uint8_t buf[2];
    if (!read_regs(dev, REG_RAW_ANG_H, buf, 2))
        return AS5600_ERR_I2C;

    *raw = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]) & ANGLE_MASK;
    return AS5600_OK;
}

as5600_err_t as5600_read_degrees(const as5600_t *dev, float *degrees)
{
    if (!dev || !degrees)
        return AS5600_ERR_PARAM;

    uint16_t raw;
    as5600_err_t err = as5600_read_raw_angle(dev, &raw);
    if (err != AS5600_OK)
        return err;

    *degrees = (float)raw * (FULL_TURN_DEG / FULL_TURN_CNT);
    return AS5600_OK;
}

as5600_err_t as5600_read_status(const as5600_t *dev, uint8_t *status)
{
    if (!dev || !status)
        return AS5600_ERR_PARAM;

    if (!read_regs(dev, REG_STATUS, status, 1))
        return AS5600_ERR_I2C;

    return AS5600_OK;
}

as5600_err_t as5600_set_start_position(const as5600_t *dev, uint16_t pos)
{
    if (!dev)
        return AS5600_ERR_PARAM;
    if (pos > ANGLE_MASK)
        return AS5600_ERR_PARAM;

    if (!write_reg(dev, REG_ZPOS_H, (uint8_t)(pos >> 8)))
        return AS5600_ERR_I2C;
    if (!write_reg(dev, REG_ZPOS_L, (uint8_t)(pos & 0xFF)))
        return AS5600_ERR_I2C;

    return AS5600_OK;
}

as5600_err_t as5600_set_max_angle(const as5600_t *dev, uint16_t angle)
{
    if (!dev)
        return AS5600_ERR_PARAM;
    if (angle > ANGLE_MASK)
        return AS5600_ERR_PARAM;

    if (!write_reg(dev, REG_MANG_H, (uint8_t)(angle >> 8)))
        return AS5600_ERR_I2C;
    if (!write_reg(dev, REG_MANG_L, (uint8_t)(angle & 0xFF)))
        return AS5600_ERR_I2C;

    return AS5600_OK;
}
