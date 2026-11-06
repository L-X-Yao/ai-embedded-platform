// SPDX-License-Identifier: MIT
/*
 * htu21d — TE HTU21D digital humidity and temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "htu21d.h"
#include "contract.h"
#include <string.h>

/* Post-measurement delay (ms) — datasheet max for 14-bit temp / 12-bit hum */
#define MEASURE_DELAY_MS  50u

/* Datasheet: up to 15 ms after power up before any command */
#define POWER_UP_DELAY_MS 15u

uint8_t htu21d_crc8(const uint8_t *data, uint16_t len)
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

htu21d_err_t htu21d_init(htu21d_t *dev, const htu21d_i2c_t *i2c)
{
    if (!dev)
    {
        return HTU21D_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c)
    {
        return HTU21D_ERR_PARAM;
    }
    if (!i2c->write_cmd || !i2c->read_data)
    {
        return HTU21D_ERR_PARAM;
    }

    dev->i2c = i2c;

    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, POWER_UP_DELAY_MS);
    }

    /* Issue soft reset */
    if (!i2c->write_cmd(i2c->ctx, HTU21D_DEV_ADDR, HTU21D_CMD_SOFT_RESET))
    {
        dev->i2c = NULL;
        return HTU21D_ERR_I2C;
    }

    /* Datasheet: 15 ms max reset time */
    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, 15);
    }

    return HTU21D_OK;
}

htu21d_err_t htu21d_read_temperature(htu21d_t *dev, float *temp_c)
{
    if (!dev || !dev->i2c || !temp_c)
    {
        return HTU21D_ERR_PARAM;
    }

    const htu21d_i2c_t *i2c = dev->i2c;

    if (!i2c->write_cmd(i2c->ctx, HTU21D_DEV_ADDR, HTU21D_CMD_TRIG_TEMP))
    {
        return HTU21D_ERR_I2C;
    }

    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, MEASURE_DELAY_MS);
    }

    uint8_t buf[3];
    if (!i2c->read_data(i2c->ctx, HTU21D_DEV_ADDR, buf, 3))
    {
        return HTU21D_ERR_I2C;
    }

    if (htu21d_crc8(buf, 2) != buf[2])
    {
        return HTU21D_ERR_CRC;
    }

    /* 14-bit: status bits [1:0] of LSB must be masked */
    uint16_t raw = (uint16_t)(((uint16_t)buf[0] << 8) | (buf[1] & 0xFC));

    *temp_c = -46.85f + 175.72f * (float)raw / 65536.0f;

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    return HTU21D_OK;
}

htu21d_err_t htu21d_read_humidity(htu21d_t *dev, float *rh_percent)
{
    if (!dev || !dev->i2c || !rh_percent)
    {
        return HTU21D_ERR_PARAM;
    }

    const htu21d_i2c_t *i2c = dev->i2c;

    if (!i2c->write_cmd(i2c->ctx, HTU21D_DEV_ADDR, HTU21D_CMD_TRIG_HUM))
    {
        return HTU21D_ERR_I2C;
    }

    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, MEASURE_DELAY_MS);
    }

    uint8_t buf[3];
    if (!i2c->read_data(i2c->ctx, HTU21D_DEV_ADDR, buf, 3))
    {
        return HTU21D_ERR_I2C;
    }

    if (htu21d_crc8(buf, 2) != buf[2])
    {
        return HTU21D_ERR_CRC;
    }

    /* 12-bit: status bits [1:0] of LSB must be masked */
    uint16_t raw = (uint16_t)(((uint16_t)buf[0] << 8) | (buf[1] & 0xFC));

    *rh_percent = -6.0f + 125.0f * (float)raw / 65536.0f;

    ENSURE(*rh_percent == *rh_percent);  /* not NaN */
    return HTU21D_OK;
}

htu21d_err_t htu21d_reset(htu21d_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return HTU21D_ERR_PARAM;
    }

    if (!dev->i2c->write_cmd(dev->i2c->ctx, HTU21D_DEV_ADDR,
                             HTU21D_CMD_SOFT_RESET))
    {
        return HTU21D_ERR_I2C;
    }

    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 15);
    }

    return HTU21D_OK;
}
