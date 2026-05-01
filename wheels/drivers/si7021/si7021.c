/*
 * si7021 — Si7021/HTU21D/SHT21 I2C temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "si7021.h"

/* --- SI7021 commands --- */
#define CMD_MEASURE_RH_NOHOLD    0xF5u
#define CMD_MEASURE_TEMP_NOHOLD  0xF3u
#define CMD_READ_TEMP_FROM_RH    0xE0u
#define CMD_RESET                0xFEu
#define CMD_WRITE_USER_REG       0xE6u
#define CMD_READ_USER_REG        0xE7u

/* Serial number read commands (two-step) */
#define CMD_READ_ID1_A           0xFAu
#define CMD_READ_ID1_B           0x0Fu
#define CMD_READ_ID2_A           0xFCu
#define CMD_READ_ID2_B           0xC9u

/* Measurement delay (ms) — worst case 14-bit: ~23 ms */
#define MEASURE_DELAY_MS         25u

/* User register resolution bits */
#define USER_REG_RES0_BIT        0x01u
#define USER_REG_RES1_BIT        0x80u
#define USER_REG_RES_MASK        (USER_REG_RES0_BIT | USER_REG_RES1_BIT)

static bool send_cmd(const si7021_t *dev, uint8_t cmd)
{
    return dev->i2c->write(dev->i2c->ctx, SI7021_DEV_ADDR, &cmd, 1);
}

static bool send_cmd2(const si7021_t *dev, uint8_t a, uint8_t b)
{
    uint8_t buf[2] = { a, b };
    return dev->i2c->write(dev->i2c->ctx, SI7021_DEV_ADDR, buf, 2);
}

uint8_t si7021_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

static si7021_err_t measure_nohold(const si7021_t *dev, uint8_t cmd,
                                   uint16_t *raw_out)
{
    if (!send_cmd(dev, cmd))
    {
        return SI7021_ERR_I2C;
    }

    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, MEASURE_DELAY_MS);
    }

    uint8_t buf[3];
    if (!dev->i2c->read(dev->i2c->ctx, SI7021_DEV_ADDR, buf, 3))
    {
        return SI7021_ERR_I2C;
    }

    if (si7021_crc8(buf, 2) != buf[2])
    {
        return SI7021_ERR_CRC;
    }

    *raw_out = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return SI7021_OK;
}

si7021_err_t si7021_init(si7021_t *dev, const si7021_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return SI7021_ERR_PARAMS;
    }
    if (!i2c->write || !i2c->read)
    {
        return SI7021_ERR_PARAMS;
    }

    dev->i2c = i2c;
    return SI7021_OK;
}

si7021_err_t si7021_read_humidity(si7021_t *dev, float *rh)
{
    if (!dev || !rh)
    {
        return SI7021_ERR_PARAMS;
    }

    uint16_t raw;
    si7021_err_t err = measure_nohold(dev, CMD_MEASURE_RH_NOHOLD, &raw);
    if (err != SI7021_OK)
    {
        return err;
    }

    *rh = 125.0f * (float)raw / 65536.0f - 6.0f;
    return SI7021_OK;
}

si7021_err_t si7021_read_temperature(si7021_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
    {
        return SI7021_ERR_PARAMS;
    }

    uint16_t raw;
    si7021_err_t err = measure_nohold(dev, CMD_MEASURE_TEMP_NOHOLD, &raw);
    if (err != SI7021_OK)
    {
        return err;
    }

    *temp_c = 175.72f * (float)raw / 65536.0f - 46.85f;
    return SI7021_OK;
}

si7021_err_t si7021_read_both(si7021_t *dev, float *rh, float *temp_c)
{
    if (!dev || !rh || !temp_c)
    {
        return SI7021_ERR_PARAMS;
    }

    /* Measure humidity (this also triggers an internal temp measurement) */
    uint16_t raw_rh;
    si7021_err_t err = measure_nohold(dev, CMD_MEASURE_RH_NOHOLD, &raw_rh);
    if (err != SI7021_OK)
    {
        return err;
    }
    *rh = 125.0f * (float)raw_rh / 65536.0f - 6.0f;

    /* Read temperature from previous RH measurement (no CRC on this one) */
    if (!send_cmd(dev, CMD_READ_TEMP_FROM_RH))
    {
        return SI7021_ERR_I2C;
    }

    uint8_t buf[2];
    if (!dev->i2c->read(dev->i2c->ctx, SI7021_DEV_ADDR, buf, 2))
    {
        return SI7021_ERR_I2C;
    }

    uint16_t raw_t = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    *temp_c = 175.72f * (float)raw_t / 65536.0f - 46.85f;
    return SI7021_OK;
}

si7021_err_t si7021_set_resolution(si7021_t *dev, si7021_resolution_t res)
{
    if (!dev)
    {
        return SI7021_ERR_PARAMS;
    }
    if ((unsigned)res > 3u)
    {
        return SI7021_ERR_PARAMS;
    }

    /* Read current user register */
    if (!send_cmd(dev, CMD_READ_USER_REG))
    {
        return SI7021_ERR_I2C;
    }

    uint8_t reg;
    if (!dev->i2c->read(dev->i2c->ctx, SI7021_DEV_ADDR, &reg, 1))
    {
        return SI7021_ERR_I2C;
    }

    /* Clear resolution bits and set new ones */
    reg = (uint8_t)(reg & (uint8_t)~USER_REG_RES_MASK);
    if (res & 1u)
    {
        reg |= USER_REG_RES0_BIT;
    }
    if (res & 2u)
    {
        reg |= USER_REG_RES1_BIT;
    }

    /* Write back */
    if (!send_cmd2(dev, CMD_WRITE_USER_REG, reg))
    {
        return SI7021_ERR_I2C;
    }

    return SI7021_OK;
}

si7021_err_t si7021_reset(si7021_t *dev)
{
    if (!dev)
    {
        return SI7021_ERR_PARAMS;
    }
    if (!send_cmd(dev, CMD_RESET))
    {
        return SI7021_ERR_I2C;
    }
    return SI7021_OK;
}

si7021_err_t si7021_read_serial(si7021_t *dev, uint32_t *serial_a,
                                uint32_t *serial_b)
{
    if (!dev || !serial_a || !serial_b)
    {
        return SI7021_ERR_PARAMS;
    }

    /* First access: send 0xFA 0x0F, read 8 bytes (SNA3, CRC, SNA2, CRC, SNA1, CRC, SNA0, CRC) */
    if (!send_cmd2(dev, CMD_READ_ID1_A, CMD_READ_ID1_B))
    {
        return SI7021_ERR_I2C;
    }

    uint8_t id1[8];
    if (!dev->i2c->read(dev->i2c->ctx, SI7021_DEV_ADDR, id1, 8))
    {
        return SI7021_ERR_I2C;
    }

    *serial_a = ((uint32_t)id1[0] << 24) | ((uint32_t)id1[2] << 16) |
                ((uint32_t)id1[4] << 8)  |  (uint32_t)id1[6];

    /* Second access: send 0xFC 0xC9, read 6 bytes (SNB3, SNB2, CRC, SNB1, SNB0, CRC) */
    if (!send_cmd2(dev, CMD_READ_ID2_A, CMD_READ_ID2_B))
    {
        return SI7021_ERR_I2C;
    }

    uint8_t id2[6];
    if (!dev->i2c->read(dev->i2c->ctx, SI7021_DEV_ADDR, id2, 6))
    {
        return SI7021_ERR_I2C;
    }

    *serial_b = ((uint32_t)id2[0] << 24) | ((uint32_t)id2[1] << 16) |
                ((uint32_t)id2[3] << 8)  |  (uint32_t)id2[4];

    return SI7021_OK;
}
