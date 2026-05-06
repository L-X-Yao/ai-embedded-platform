// SPDX-License-Identifier: MIT
/*
 * lps22hb — ST LPS22HB barometric pressure sensor driver (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Conversion per LPS22HB datasheet (DocID027112):
 *   pressure = raw / 4096.0 (hPa)
 *   temperature = raw / 100.0 (deg C)
 */

#include "lps22hb.h"
#include "contract.h"

/* ---------- register map ---------- */

#define REG_WHO_AM_I    0x0Fu
#define REG_CTRL_REG1   0x10u
#define REG_CTRL_REG2   0x11u
#define REG_STATUS      0x27u
#define REG_PRESS_XL    0x28u
#define REG_PRESS_L     0x29u
#define REG_PRESS_H     0x2Au
#define REG_TEMP_L      0x2Bu
#define REG_TEMP_H      0x2Cu

/* CTRL_REG2 bits */
#define CTRL_REG2_ONE_SHOT  0x01u

/* STATUS bits */
#define STATUS_P_DA  0x01u  /* pressure data available  */
#define STATUS_T_DA  0x02u  /* temperature data available */

/* ---------- internal helpers ---------- */

static bool write_reg(const lps22hb_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, reg, val);
}

static bool read_regs(const lps22hb_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, len);
}

/* ---------- API ---------- */

lps22hb_err_t lps22hb_init(lps22hb_t *dev, const lps22hb_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return LPS22HB_ERR_PARAM;
    }

    dev->i2c  = i2c;
    dev->addr = LPS22HB_DEV_ADDR;

    /* verify chip identity */
    uint8_t id = 0;
    if (!read_regs(dev, REG_WHO_AM_I, &id, 1))
    {
        return LPS22HB_ERR_I2C;
    }
    if (id != LPS22HB_CHIP_ID)
    {
        return LPS22HB_ERR_ID;
    }

    /* default: power-down / one-shot, BDU=1 (bit 1) for coherent reads */
    if (!write_reg(dev, REG_CTRL_REG1, 0x02))
    {
        return LPS22HB_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return LPS22HB_OK;
}

lps22hb_err_t lps22hb_read(lps22hb_t *dev, lps22hb_data_t *out)
{
    if (!dev || !out)
    {
        return LPS22HB_ERR_PARAM;
    }

    /* read 5 bytes: PRESS_XL, PRESS_L, PRESS_H, TEMP_L, TEMP_H */
    uint8_t buf[5];
    if (!read_regs(dev, REG_PRESS_XL, buf, 5))
    {
        return LPS22HB_ERR_I2C;
    }

    /* 24-bit two's complement pressure */
    int32_t raw_p = (int32_t)((uint32_t)buf[2] << 16 |
                              (uint32_t)buf[1] << 8  |
                              (uint32_t)buf[0]);
    /* sign-extend from 24-bit */
    if (raw_p & 0x00800000)
    {
        raw_p |= (int32_t)0xFF000000;
    }

    /* 16-bit two's complement temperature */
    int16_t raw_t = (int16_t)((uint16_t)buf[4] << 8 | (uint16_t)buf[3]);

    out->pressure_hpa  = (float)raw_p / 4096.0f;
    out->temperature_c = (float)raw_t / 100.0f;

    ENSURE(out->pressure_hpa == out->pressure_hpa);     /* not NaN */
    ENSURE(out->temperature_c == out->temperature_c);    /* not NaN */
    return LPS22HB_OK;
}

lps22hb_err_t lps22hb_set_odr(lps22hb_t *dev, lps22hb_odr_t odr)
{
    if (!dev)
    {
        return LPS22HB_ERR_PARAM;
    }
    if ((unsigned)odr > 5)
    {
        return LPS22HB_ERR_PARAM;
    }

    /* CTRL_REG1: ODR[6:4], BDU=1 (bit 1) */
    uint8_t val = (uint8_t)(((uint8_t)odr << 4) | 0x02u);
    if (!write_reg(dev, REG_CTRL_REG1, val))
    {
        return LPS22HB_ERR_I2C;
    }

    return LPS22HB_OK;
}

lps22hb_err_t lps22hb_one_shot_trigger(lps22hb_t *dev)
{
    if (!dev)
    {
        return LPS22HB_ERR_PARAM;
    }

    /* read current CTRL_REG2 to preserve other bits */
    uint8_t reg2 = 0;
    if (!read_regs(dev, REG_CTRL_REG2, &reg2, 1))
    {
        return LPS22HB_ERR_I2C;
    }

    reg2 |= CTRL_REG2_ONE_SHOT;
    if (!write_reg(dev, REG_CTRL_REG2, reg2))
    {
        return LPS22HB_ERR_I2C;
    }

    return LPS22HB_OK;
}

lps22hb_err_t lps22hb_read_raw_pressure(lps22hb_t *dev, int32_t *raw)
{
    if (!dev || !raw)
    {
        return LPS22HB_ERR_PARAM;
    }

    uint8_t buf[3];
    if (!read_regs(dev, REG_PRESS_XL, buf, 3))
    {
        return LPS22HB_ERR_I2C;
    }

    int32_t val = (int32_t)((uint32_t)buf[2] << 16 |
                            (uint32_t)buf[1] << 8  |
                            (uint32_t)buf[0]);
    /* sign-extend from 24-bit */
    if (val & 0x00800000)
    {
        val |= (int32_t)0xFF000000;
    }

    *raw = val;
    return LPS22HB_OK;
}

lps22hb_err_t lps22hb_read_raw_temperature(lps22hb_t *dev, int16_t *raw)
{
    if (!dev || !raw)
    {
        return LPS22HB_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP_L, buf, 2))
    {
        return LPS22HB_ERR_I2C;
    }

    *raw = (int16_t)((uint16_t)buf[1] << 8 | (uint16_t)buf[0]);
    return LPS22HB_OK;
}
