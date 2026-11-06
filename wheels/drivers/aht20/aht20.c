// SPDX-License-Identifier: MIT
/*
 * aht20 — ASAIR AHT20 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "aht20.h"
#include "contract.h"
#include <string.h>

/* ---- commands ---- */
#define CMD_INIT_0          0xBE
#define CMD_INIT_1          0x08
#define CMD_INIT_2          0x00

#define CMD_TRIGGER_0       0xAC
#define CMD_TRIGGER_1       0x33
#define CMD_TRIGGER_2       0x00

#define CMD_SOFT_RESET      0xBA

/* ---- status bits ---- */
#define STATUS_BUSY_BIT     (1U << 7)
#define STATUS_CAL_BIT      (1U << 3)

/* ---- timing ---- */
#define DELAY_POWER_ON_MS   40
#define DELAY_MEASURE_MS    80
#define DELAY_RESET_MS      20
#define DELAY_BUSY_POLL_MS  10
#define MAX_BUSY_POLLS      20    /* no limit in the manual: 200 ms past the 80 */

/* ---- conversion ---- */
#define RAW_DIVISOR         1048576.0f   /* 2^20 */

/* ---- helpers ---- */

static bool send_cmd1(const aht20_t *dev, uint8_t byte)
{
    uint8_t cmd[1] = { byte };
    return dev->i2c->write(dev->i2c->ctx, AHT20_ADDR, cmd, 1);
}

static bool send_cmd3(const aht20_t *dev, uint8_t b0, uint8_t b1, uint8_t b2)
{
    uint8_t cmd[3] = { b0, b1, b2 };
    return dev->i2c->write(dev->i2c->ctx, AHT20_ADDR, cmd, 3);
}

static void delay(const aht20_t *dev, uint16_t ms)
{
    dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

static bool read_status(aht20_t *dev)
{
    uint8_t st;
    if (!dev->i2c->read(dev->i2c->ctx, AHT20_ADDR, &st, 1))
    {
        return false;
    }
    dev->status = st;
    return true;
}

/* ---- public API ---- */

uint8_t aht20_crc8(const uint8_t *data, uint16_t len)
{
    if (!data)
    {
        return 0;
    }
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

aht20_err_t aht20_init(aht20_t *dev, const aht20_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return AHT20_ERR_PARAM;
    }
    if (!i2c->write || !i2c->read || !i2c->delay_ms)
    {
        return AHT20_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* wait for power-on */
    delay(dev, DELAY_POWER_ON_MS);

    /* read status to check calibration */
    if (!read_status(dev))
    {
        return AHT20_ERR_I2C;
    }

    /* if not calibrated, send init command */
    if (!(dev->status & STATUS_CAL_BIT))
    {
        if (!send_cmd3(dev, CMD_INIT_0, CMD_INIT_1, CMD_INIT_2))
        {
            return AHT20_ERR_I2C;
        }
        delay(dev, DELAY_POWER_ON_MS);

        /* re-read status to confirm calibration */
        if (!read_status(dev))
        {
            return AHT20_ERR_I2C;
        }
        if (!(dev->status & STATUS_CAL_BIT))
        {
            return AHT20_ERR_NOT_CAL;
        }
    }

    return AHT20_OK;
}

aht20_err_t aht20_read(aht20_t *dev, aht20_data_t *out)
{
    if (!dev || !dev->i2c || !out)
    {
        return AHT20_ERR_PARAM;
    }

    /* trigger measurement */
    if (!send_cmd3(dev, CMD_TRIGGER_0, CMD_TRIGGER_1, CMD_TRIGGER_2))
    {
        return AHT20_ERR_I2C;
    }

    delay(dev, DELAY_MEASURE_MS);

    /* read 7 bytes: status + 5 data bytes + CRC, until bit 7 says it is done.
     * Only then do the data and the CRC mean anything. */
    uint8_t buf[7];
    for (uint8_t polls = 0; ; polls++)
    {
        if (!dev->i2c->read(dev->i2c->ctx, AHT20_ADDR, buf, 7))
        {
            return AHT20_ERR_I2C;
        }
        if (!(buf[0] & STATUS_BUSY_BIT))
        {
            break;
        }
        if (polls >= MAX_BUSY_POLLS)
        {
            return AHT20_ERR_TIMEOUT;
        }
        delay(dev, DELAY_BUSY_POLL_MS);
    }

    /* verify CRC over first 6 bytes (status + data) */
    if (aht20_crc8(buf, 6) != buf[6])
    {
        return AHT20_ERR_CRC;
    }

    dev->status = buf[0];

    /*
     * Data layout (6 bytes after status):
     *   buf[1]       = humidity[19:12]
     *   buf[2]       = humidity[11:4]
     *   buf[3] upper = humidity[3:0]
     *   buf[3] lower = temperature[19:16]
     *   buf[4]       = temperature[15:8]
     *   buf[5]       = temperature[7:0]
     */
    uint32_t raw_h = ((uint32_t)buf[1] << 12)
                   | ((uint32_t)buf[2] << 4)
                   | ((uint32_t)(buf[3] >> 4));
    uint32_t raw_t = (((uint32_t)(buf[3] & 0x0F)) << 16)
                   | ((uint32_t)buf[4] << 8)
                   | ((uint32_t)buf[5]);

    out->humidity_pct   = (float)raw_h / RAW_DIVISOR * 100.0f;
    out->temperature_c  = (float)raw_t / RAW_DIVISOR * 200.0f - 50.0f;

    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */
    ENSURE(out->humidity_pct == out->humidity_pct);      /* not NaN */
    return AHT20_OK;
}

aht20_err_t aht20_reset(aht20_t *dev)
{
    if (!dev || !dev->i2c)
    {
        return AHT20_ERR_PARAM;
    }
    if (!send_cmd1(dev, CMD_SOFT_RESET))
    {
        return AHT20_ERR_I2C;
    }
    delay(dev, DELAY_RESET_MS);
    return AHT20_OK;
}

bool aht20_is_calibrated(aht20_t *dev)
{
    if (!dev)
    {
        return false;
    }
    return (dev->status & STATUS_CAL_BIT) != 0;
}
