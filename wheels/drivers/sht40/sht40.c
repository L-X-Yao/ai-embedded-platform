// SPDX-License-Identifier: MIT
/*
 * sht40 — Sensirion SHT40 temperature and humidity sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "sht40.h"
#include "contract.h"
#include <string.h>

/* ---------- SHT40 commands (single-byte) ---------- */

#define CMD_MEASURE_HIGH    0xFDu
#define CMD_MEASURE_MEDIUM  0xF6u
#define CMD_MEASURE_LOW     0xE0u
#define CMD_READ_SERIAL     0x89u
#define CMD_SOFT_RESET      0x94u

/* ---------- internal helpers ---------- */

uint8_t sht40_crc8(const uint8_t *data, uint16_t len)
{
    if (!data) return 0;
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

static bool send_cmd(const sht40_t *dev, uint8_t cmd)
{
    return dev->i2c->write(dev->i2c->ctx, dev->addr, &cmd, 1);
}

static bool read_response(const sht40_t *dev, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read(dev->i2c->ctx, dev->addr, buf, len);
}

static float clamp_f(float val, float lo, float hi)
{
    if (!(val >= lo)) return lo;  /* NaN-safe: NaN falls to lo */
    if (val > hi) return hi;
    return val;
}

static sht40_err_t measure(sht40_t *dev, uint8_t cmd, uint16_t wait_ms,
                           sht40_data_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return SHT40_ERR_PARAM;
    }

    if (!send_cmd(dev, cmd))
    {
        return SHT40_ERR_I2C;
    }

    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, wait_ms);
    }

    uint8_t buf[6];
    if (!read_response(dev, buf, 6))
    {
        return SHT40_ERR_I2C;
    }

    /* Verify CRC for temperature word */
    if (sht40_crc8(&buf[0], 2) != buf[2])
    {
        return SHT40_ERR_CRC;
    }

    /* Verify CRC for humidity word */
    if (sht40_crc8(&buf[3], 2) != buf[5])
    {
        return SHT40_ERR_CRC;
    }

    uint16_t raw_t = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    uint16_t raw_h = (uint16_t)((uint16_t)buf[3] << 8 | buf[4]);

    out->temperature_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    out->humidity_pct  = -6.0f + 125.0f * (float)raw_h / 65535.0f;
    out->humidity_pct  = clamp_f(out->humidity_pct, 0.0f, 100.0f);

    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */
    ENSURE(out->humidity_pct == out->humidity_pct);      /* not NaN */

    return SHT40_OK;
}

/* ---------- public API ---------- */

sht40_err_t sht40_init(sht40_t *dev, const sht40_i2c_t *i2c, uint8_t addr)
{
    if (!dev)
    {
        return SHT40_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c)
    {
        return SHT40_ERR_PARAM;
    }
    if (!i2c->write || !i2c->read)
    {
        return SHT40_ERR_PARAM;
    }

    dev->i2c  = i2c;
    dev->addr = addr;

    /* Soft reset */
    if (!send_cmd(dev, CMD_SOFT_RESET))
    {
        dev->i2c = NULL;
        return SHT40_ERR_I2C;
    }

    if (i2c->delay_ms)
    {
        i2c->delay_ms(i2c->ctx, 1);  /* reset time ~1ms */
    }

    /* Read serial number to verify communication */
    uint32_t serial;
    sht40_err_t err = sht40_read_serial(dev, &serial);
    if (err != SHT40_OK)
    {
        dev->i2c = NULL;
        return err;
    }
    dev->serial = serial;

    return SHT40_OK;
}

sht40_err_t sht40_read_high(sht40_t *dev, sht40_data_t *out)
{
    return measure(dev, CMD_MEASURE_HIGH, 10, out);
}

sht40_err_t sht40_read_medium(sht40_t *dev, sht40_data_t *out)
{
    return measure(dev, CMD_MEASURE_MEDIUM, 5, out);
}

sht40_err_t sht40_read_low(sht40_t *dev, sht40_data_t *out)
{
    return measure(dev, CMD_MEASURE_LOW, 2, out);
}

sht40_err_t sht40_soft_reset(sht40_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return SHT40_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_SOFT_RESET))
    {
        return SHT40_ERR_I2C;
    }

    /* p.10 tSR: up to 1 ms from the ACK of the command to the idle state */
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 1);
    }
    return SHT40_OK;
}

sht40_err_t sht40_read_serial(sht40_t *dev, uint32_t *serial)
{
    if (!dev || !dev->i2c || !serial)
    {
        return SHT40_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_READ_SERIAL))
    {
        return SHT40_ERR_I2C;
    }

    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 1);
    }

    uint8_t buf[6];
    if (!read_response(dev, buf, 6))
    {
        return SHT40_ERR_I2C;
    }

    if (sht40_crc8(&buf[0], 2) != buf[2])
    {
        return SHT40_ERR_CRC;
    }
    if (sht40_crc8(&buf[3], 2) != buf[5])
    {
        return SHT40_ERR_CRC;
    }

    *serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
              ((uint32_t)buf[3] << 8)  | (uint32_t)buf[4];

    return SHT40_OK;
}
