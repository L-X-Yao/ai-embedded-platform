// SPDX-License-Identifier: MIT
/*
 * dps310 — Infineon DPS310 barometric pressure and temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithm from the Infineon DPS310 datasheet.
 */

#include "dps310.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_PSR_B2     0x00u  /* pressure MSB     */
#define REG_PSR_B1     0x01u  /* pressure middle   */
#define REG_PSR_B0     0x02u  /* pressure LSB      */
#define REG_TMP_B2     0x03u  /* temperature MSB   */
#define REG_TMP_B1     0x04u  /* temperature middle*/
#define REG_TMP_B0     0x05u  /* temperature LSB   */
#define REG_PRS_CFG    0x06u  /* pressure config   */
#define REG_TMP_CFG    0x07u  /* temperature config*/
#define REG_MEAS_CFG   0x08u  /* measurement mode  */
#define REG_CFG_REG    0x09u  /* interrupt / FIFO  */
#define REG_RESET      0x0Cu  /* soft reset        */
#define REG_ID         0x0Du  /* product ID        */
#define REG_COEF_START 0x10u  /* calibration start */
#define REG_COEF_SRCE  0x28u  /* coefficient source */

#define ID_PROD_MASK   0x0Fu  /* PROD_ID; REV_ID is bits 7:4 */
#define RESET_CMD      0x89u
#define COEF_LEN       18u

/* Bit 7 in both TMP_COEF_SRCE and TMP_CFG (TMP_EXT): MEMS element sensor */
#define TMP_SENSOR_MEMS 0x80u

/* MEAS_CFG bits */
#define MEAS_CFG_COEF_RDY   0x80u
#define MEAS_CFG_SENSOR_RDY 0x40u
#define MEAS_CFG_START_UP (MEAS_CFG_COEF_RDY | MEAS_CFG_SENSOR_RDY)
#define MEAS_CFG_TMP_RDY  0x20u
#define MEAS_CFG_PRS_RDY  0x10u
#define MEAS_CFG_CMD_TEMP 0x02u  /* single temperature measurement */
#define MEAS_CFG_CMD_PRS  0x01u  /* single pressure measurement   */

/* MEAS_CFG polls in the start-up wait. At 1 ms each this is far past the
 * 40 ms of Table 8. With delay_ms NULL only the bus time paces it, about
 * 48 ms at 400 kHz, so a faster bus can time out on a cold boot. */
#define START_UP_TRIES 500

/* ---------- scale factor table ---------- */

/*
 * Oversampling scale factors from DPS310 datasheet.
 * Index = oversampling enum value (0..6 maps to 1x..64x).
 */
static const float scale_factors[] = {
    524288.0f,   /* 1x   */
    1572864.0f,  /* 2x   */
    3670016.0f,  /* 4x   */
    7864320.0f,  /* 8x   */
    253952.0f,   /* 16x  */
    516096.0f,   /* 32x  */
    1040384.0f,  /* 64x  */
};

#define SCALE_FACTOR_COUNT 7

/* ---------- internal helpers ---------- */

static bool write_reg(const dps310_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const dps310_t *dev, uint8_t reg, uint8_t *buf,
                      uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

/*
 * Sign-extend a 24-bit two's complement value to int32_t.
 */
static int32_t sign_extend_24(uint32_t raw)
{
    if (raw & 0x800000u)
    {
        return (int32_t)(raw | 0xFF000000u);
    }
    return (int32_t)raw;
}

/*
 * Wait for COEF_RDY and SENSOR_RDY. Table 8 allows 40 ms and 12 ms after
 * power-on or soft reset, and the coefficients are not valid before that.
 */
static dps310_err_t wait_start_up(const dps310_t *dev)
{
    for (int tries = 0; tries < START_UP_TRIES; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_MEAS_CFG, &status, 1))
        {
            return DPS310_ERR_I2C;
        }
        if ((status & MEAS_CFG_START_UP) == MEAS_CFG_START_UP)
        {
            return DPS310_OK;
        }
        if (dev->i2c->delay_ms)
        {
            dev->i2c->delay_ms(dev->i2c->ctx, 1);
        }
    }
    return DPS310_ERR_TIMEOUT;
}

/*
 * Parse calibration coefficients from 18 bytes at registers 0x10-0x21.
 *
 * DPS310 coefficient layout:
 *   c0:  reg[0x10][7:0] | reg[0x11][7:4]  -> 12-bit two's complement
 *   c1:  reg[0x11][3:0] | reg[0x12][7:0]  -> 12-bit two's complement
 *   c00: reg[0x13][7:0] | reg[0x14][7:0] | reg[0x15][7:4]  -> 20-bit two's complement
 *   c10: reg[0x15][3:0] | reg[0x16][7:0] | reg[0x17][7:0]  -> 20-bit two's complement
 *   c01: reg[0x18][7:0] | reg[0x19][7:0]  -> 16-bit two's complement
 *   c11: reg[0x1A][7:0] | reg[0x1B][7:0]  -> 16-bit two's complement
 *   c20: reg[0x1C][7:0] | reg[0x1D][7:0]  -> 16-bit two's complement
 *   c21: reg[0x1E][7:0] | reg[0x1F][7:0]  -> 16-bit two's complement
 *   c30: reg[0x20][7:0] | reg[0x21][7:0]  -> 16-bit two's complement
 */
static dps310_err_t read_calibration(dps310_t *dev)
{
    uint8_t buf[COEF_LEN];
    if (!read_regs(dev, REG_COEF_START, buf, COEF_LEN))
    {
        return DPS310_ERR_I2C;
    }

    /* c0: 12-bit two's complement */
    uint16_t c0_raw = (uint16_t)(((uint16_t)buf[0] << 4) |
                                  (((uint16_t)buf[1] >> 4) & 0x0Fu));
    if (c0_raw & 0x0800u)
    {
        dev->calib.c0 = (float)(int16_t)(c0_raw | 0xF000u);
    }
    else
    {
        dev->calib.c0 = (float)c0_raw;
    }

    /* c1: 12-bit two's complement */
    uint16_t c1_raw = (uint16_t)((((uint16_t)buf[1] & 0x0Fu) << 8) |
                                  (uint16_t)buf[2]);
    if (c1_raw & 0x0800u)
    {
        dev->calib.c1 = (float)(int16_t)(c1_raw | 0xF000u);
    }
    else
    {
        dev->calib.c1 = (float)c1_raw;
    }

    /* c00: 20-bit two's complement */
    uint32_t c00_raw = ((uint32_t)buf[3] << 12) |
                       ((uint32_t)buf[4] << 4) |
                       (((uint32_t)buf[5] >> 4) & 0x0Fu);
    if (c00_raw & 0x80000u)
    {
        dev->calib.c00 = (float)(int32_t)(c00_raw | 0xFFF00000u);
    }
    else
    {
        dev->calib.c00 = (float)c00_raw;
    }

    /* c10: 20-bit two's complement */
    uint32_t c10_raw = (((uint32_t)buf[5] & 0x0Fu) << 16) |
                       ((uint32_t)buf[6] << 8) |
                       (uint32_t)buf[7];
    if (c10_raw & 0x80000u)
    {
        dev->calib.c10 = (float)(int32_t)(c10_raw | 0xFFF00000u);
    }
    else
    {
        dev->calib.c10 = (float)c10_raw;
    }

    /* c01: 16-bit two's complement */
    dev->calib.c01 = (float)(int16_t)((uint16_t)((uint16_t)buf[8] << 8) |
                                       (uint16_t)buf[9]);

    /* c11: 16-bit two's complement */
    dev->calib.c11 = (float)(int16_t)((uint16_t)((uint16_t)buf[10] << 8) |
                                       (uint16_t)buf[11]);

    /* c20: 16-bit two's complement */
    dev->calib.c20 = (float)(int16_t)((uint16_t)((uint16_t)buf[12] << 8) |
                                       (uint16_t)buf[13]);

    /* c21: 16-bit two's complement */
    dev->calib.c21 = (float)(int16_t)((uint16_t)((uint16_t)buf[14] << 8) |
                                       (uint16_t)buf[15]);

    /* c30: 16-bit two's complement */
    dev->calib.c30 = (float)(int16_t)((uint16_t)((uint16_t)buf[16] << 8) |
                                       (uint16_t)buf[17]);

    return DPS310_OK;
}

/*
 * Temperature compensation (DPS310 datasheet):
 *   T_raw_sc = T_raw / kT
 *   T_comp   = c0 * 0.5 + c1 * T_raw_sc
 */
static float compensate_temperature(const dps310_t *dev, int32_t raw_temp)
{
    float t_raw_sc = (float)raw_temp / dev->kt;
    return dev->calib.c0 * 0.5f + dev->calib.c1 * t_raw_sc;
}

/*
 * Pressure compensation (DPS310 datasheet):
 *   P_raw_sc = P_raw / kP
 *   P_comp   = c00 + P_raw_sc * (c10 + P_raw_sc * (c20 + P_raw_sc * c30))
 *            + T_raw_sc * (c01 + P_raw_sc * (c11 + P_raw_sc * c21))
 */
static float compensate_pressure(const dps310_t *dev, int32_t raw_press,
                                 int32_t raw_temp)
{
    float p_raw_sc = (float)raw_press / dev->kp;
    float t_raw_sc = (float)raw_temp / dev->kt;

    float press = dev->calib.c00
                + p_raw_sc * (dev->calib.c10
                    + p_raw_sc * (dev->calib.c20
                        + p_raw_sc * dev->calib.c30))
                + t_raw_sc * (dev->calib.c01
                    + p_raw_sc * (dev->calib.c11
                        + p_raw_sc * dev->calib.c21));

    return press;
}

/* ---------- public API ---------- */

dps310_err_t dps310_init(dps310_t *dev, const dps310_i2c_t *i2c,
                         const dps310_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return DPS310_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs)
    {
        return DPS310_ERR_PARAM;
    }
    if ((unsigned)cfg->osrs_t >= SCALE_FACTOR_COUNT ||
        (unsigned)cfg->osrs_p >= SCALE_FACTOR_COUNT)
    {
        return DPS310_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Set scale factors from oversampling config */
    dev->kt = scale_factors[(unsigned)cfg->osrs_t];
    dev->kp = scale_factors[(unsigned)cfg->osrs_p];

    /* Verify product ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_ID, &id, 1))
    {
        return DPS310_ERR_I2C;
    }
    if ((id & ID_PROD_MASK) != (DPS310_PRODUCT_ID & ID_PROD_MASK))
    {
        return DPS310_ERR_ID;
    }

    /* The coefficients can only be read once start-up has finished */
    dps310_err_t err = wait_start_up(dev);
    if (err != DPS310_OK)
    {
        return err;
    }

    /* Read calibration coefficients */
    err = read_calibration(dev);
    if (err != DPS310_OK)
    {
        return err;
    }

    /* Configure pressure oversampling: PRS_CFG register 0x06 bits [2:0] */
    uint8_t prs_cfg = (uint8_t)(cfg->osrs_p & 0x07u);
    if (!write_reg(dev, REG_PRS_CFG, prs_cfg))
    {
        return DPS310_ERR_I2C;
    }

    /* The coefficients are valid for one temperature sensor only */
    uint8_t coef_srce = 0;
    if (!read_regs(dev, REG_COEF_SRCE, &coef_srce, 1))
    {
        return DPS310_ERR_I2C;
    }

    /* Configure temperature oversampling: TMP_CFG register 0x07 bits [2:0]
     * bit 7 = TMP_EXT, the sensor the coefficients are based on */
    uint8_t tmp_cfg = (uint8_t)((coef_srce & TMP_SENSOR_MEMS) |
                                (cfg->osrs_t & 0x07u));
    if (!write_reg(dev, REG_TMP_CFG, tmp_cfg))
    {
        return DPS310_ERR_I2C;
    }

    /* If oversampling > 8x, set P_SHIFT / T_SHIFT in CFG_REG */
    uint8_t cfg_reg = 0;
    if ((unsigned)cfg->osrs_p > 3)
    {
        cfg_reg |= 0x04u;  /* P_SHIFT */
    }
    if ((unsigned)cfg->osrs_t > 3)
    {
        cfg_reg |= 0x08u;  /* T_SHIFT */
    }
    if (cfg_reg != 0)
    {
        if (!write_reg(dev, REG_CFG_REG, cfg_reg))
        {
            return DPS310_ERR_I2C;
        }
    }

    INVARIANT(dev->i2c != NULL);
    return DPS310_OK;
}

dps310_err_t dps310_read(dps310_t *dev, dps310_data_t *out)
{
    if (!dev || !out)
    {
        return DPS310_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return DPS310_ERR_PARAM;
    }

    /* Trigger temperature measurement (command mode) */
    if (!write_reg(dev, REG_MEAS_CFG, MEAS_CFG_CMD_TEMP))
    {
        return DPS310_ERR_I2C;
    }

    /* Wait for temperature ready */
    for (int tries = 0; tries < 500; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_MEAS_CFG, &status, 1))
        {
            return DPS310_ERR_I2C;
        }
        if (status & MEAS_CFG_TMP_RDY)
        {
            break;
        }
        if (tries == 499)
        {
            return DPS310_ERR_I2C;
        }
        if (dev->i2c->delay_ms)
        {
            dev->i2c->delay_ms(dev->i2c->ctx, 1);
        }
    }

    /* Read 3 bytes temperature (regs 0x03-0x05) */
    uint8_t tmp_buf[3];
    if (!read_regs(dev, REG_TMP_B2, tmp_buf, 3))
    {
        return DPS310_ERR_I2C;
    }

    uint32_t raw_temp_u = ((uint32_t)tmp_buf[0] << 16) |
                          ((uint32_t)tmp_buf[1] << 8) |
                          (uint32_t)tmp_buf[2];
    int32_t raw_temp = sign_extend_24(raw_temp_u);

    /* Trigger pressure measurement (command mode) */
    if (!write_reg(dev, REG_MEAS_CFG, MEAS_CFG_CMD_PRS))
    {
        return DPS310_ERR_I2C;
    }

    /* Wait for pressure ready */
    for (int tries = 0; tries < 500; tries++)
    {
        uint8_t status;
        if (!read_regs(dev, REG_MEAS_CFG, &status, 1))
        {
            return DPS310_ERR_I2C;
        }
        if (status & MEAS_CFG_PRS_RDY)
        {
            break;
        }
        if (tries == 499)
        {
            return DPS310_ERR_I2C;
        }
        if (dev->i2c->delay_ms)
        {
            dev->i2c->delay_ms(dev->i2c->ctx, 1);
        }
    }

    /* Read 3 bytes pressure (regs 0x00-0x02) */
    uint8_t prs_buf[3];
    if (!read_regs(dev, REG_PSR_B2, prs_buf, 3))
    {
        return DPS310_ERR_I2C;
    }

    uint32_t raw_press_u = ((uint32_t)prs_buf[0] << 16) |
                           ((uint32_t)prs_buf[1] << 8) |
                           (uint32_t)prs_buf[2];
    int32_t raw_press = sign_extend_24(raw_press_u);

    /* Compensate */
    out->temperature_c = compensate_temperature(dev, raw_temp);
    out->pressure_pa   = compensate_pressure(dev, raw_press, raw_temp);

    /* NaN guard — range validation belongs in the application, not the driver */
    ENSURE(out->temperature_c == out->temperature_c);
    ENSURE(out->pressure_pa == out->pressure_pa);
    return DPS310_OK;
}

dps310_err_t dps310_reset(dps310_t *dev)
{
    if (!dev)
    {
        return DPS310_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return DPS310_ERR_PARAM;
    }
    if (!write_reg(dev, REG_RESET, RESET_CMD))
    {
        return DPS310_ERR_I2C;
    }
    return DPS310_OK;
}
