/*
 * sht30 — Sensirion SHT30 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "sht30.h"
#include <string.h>

#define CMD_SINGLE_HIGH_MSB   0x24
#define CMD_SINGLE_HIGH_LSB   0x00
#define CMD_SOFT_RESET_MSB    0x30
#define CMD_SOFT_RESET_LSB    0xA2

static bool send_cmd(const sht30_t *dev, uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return dev->i2c.write(dev->i2c.ctx, dev->cfg.addr, cmd, 2);
}

uint8_t sht30_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
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

sht30_err_t sht30_init(sht30_t *dev, const sht30_i2c_t *i2c,
                       const sht30_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return SHT30_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    if (!send_cmd(dev, CMD_SOFT_RESET_MSB, CMD_SOFT_RESET_LSB))
    {
        return SHT30_ERR_I2C;
    }

    return SHT30_OK;
}

sht30_err_t sht30_read(sht30_t *dev, sht30_data_t *out)
{
    if (!dev || !out)
    {
        return SHT30_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_SINGLE_HIGH_MSB, CMD_SINGLE_HIGH_LSB))
    {
        return SHT30_ERR_I2C;
    }

    if (dev->i2c.delay_ms)
    {
        dev->i2c.delay_ms(dev->i2c.ctx, 16);
    }

    uint8_t buf[6];
    if (!dev->i2c.read(dev->i2c.ctx, dev->cfg.addr, buf, 6))
    {
        return SHT30_ERR_I2C;
    }

    if (sht30_crc8(&buf[0], 2) != buf[2])
    {
        return SHT30_ERR_CRC;
    }
    if (sht30_crc8(&buf[3], 2) != buf[5])
    {
        return SHT30_ERR_CRC;
    }

    uint16_t raw_t = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    uint16_t raw_h = (uint16_t)((uint16_t)buf[3] << 8 | buf[4]);

    out->temperature_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    out->humidity_pct   = 100.0f * (float)raw_h / 65535.0f;

    return SHT30_OK;
}

sht30_err_t sht30_reset(sht30_t *dev)
{
    if (!dev)
    {
        return SHT30_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_SOFT_RESET_MSB, CMD_SOFT_RESET_LSB))
    {
        return SHT30_ERR_I2C;
    }
    return SHT30_OK;
}
