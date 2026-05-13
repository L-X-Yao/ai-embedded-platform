// SPDX-License-Identifier: MIT
/*
 * shtc3 — Sensirion SHTC3 low-power humidity and temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command-based I2C protocol with CRC-8 protected responses.
 * Same Sensirion CRC-8 as SHT30 (poly 0x31, init 0xFF).
 */

#include "shtc3.h"
#include "contract.h"
#include <string.h>

/* 16-bit commands (MSB first) */
#define CMD_WAKEUP_MSB        0x35
#define CMD_WAKEUP_LSB        0x17
#define CMD_SLEEP_MSB         0xB0
#define CMD_SLEEP_LSB         0x98
#define CMD_READ_ID_MSB       0xEF
#define CMD_READ_ID_LSB       0xC8
#define CMD_RESET_MSB         0x80
#define CMD_RESET_LSB         0x5D
#define CMD_MEASURE_NP_MSB    0x78  /* Normal power, T-first */
#define CMD_MEASURE_NP_LSB    0x66

/* Chip ID verification: response bits masked with 0x083F should equal 0x0807 */
#define SHTC3_ID_MASK         0x083F
#define SHTC3_ID_EXPECT       0x0807

static bool send_cmd(const shtc3_t *dev, uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return dev->i2c->write(dev->i2c->ctx, SHTC3_ADDR, cmd, 2);
}

uint8_t shtc3_crc8(const uint8_t *data, uint16_t len)
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

shtc3_err_t shtc3_init(shtc3_t *dev, const shtc3_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!i2c->write || !i2c->read)
    {
        return SHTC3_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* Wakeup the sensor */
    if (!send_cmd(dev, CMD_WAKEUP_MSB, CMD_WAKEUP_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    /* Short delay for wakeup (datasheet: ~240 us) */
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 1);
    }

    /* Read chip ID */
    if (!send_cmd(dev, CMD_READ_ID_MSB, CMD_READ_ID_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    uint8_t id_buf[3];  /* 2 bytes ID + 1 byte CRC */
    if (!dev->i2c->read(dev->i2c->ctx, SHTC3_ADDR, id_buf, 3))
    {
        return SHTC3_ERR_I2C;
    }

    /* Verify CRC of ID response */
    if (shtc3_crc8(&id_buf[0], 2) != id_buf[2])
    {
        return SHTC3_ERR_CRC;
    }

    /* Verify chip ID: mask and compare */
    uint16_t chip_id = (uint16_t)((uint16_t)id_buf[0] << 8 | id_buf[1]);
    if ((chip_id & SHTC3_ID_MASK) != SHTC3_ID_EXPECT)
    {
        return SHTC3_ERR_ID;
    }

    /* Put sensor to sleep */
    if (!send_cmd(dev, CMD_SLEEP_MSB, CMD_SLEEP_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return SHTC3_OK;
}

shtc3_err_t shtc3_read(shtc3_t *dev, shtc3_data_t *out)
{
    if (!dev || !out)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return SHTC3_ERR_PARAM;
    }

    /* Wakeup */
    if (!send_cmd(dev, CMD_WAKEUP_MSB, CMD_WAKEUP_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 1);
    }

    /* Trigger measurement (normal power, T-first) */
    if (!send_cmd(dev, CMD_MEASURE_NP_MSB, CMD_MEASURE_NP_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    /* Wait for measurement to complete (~12 ms for normal power) */
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, 13);
    }

    /* Read 6 bytes: temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC */
    uint8_t buf[6];
    if (!dev->i2c->read(dev->i2c->ctx, SHTC3_ADDR, buf, 6))
    {
        return SHTC3_ERR_I2C;
    }

    /* Verify CRC for temperature */
    if (shtc3_crc8(&buf[0], 2) != buf[2])
    {
        return SHTC3_ERR_CRC;
    }

    /* Verify CRC for humidity */
    if (shtc3_crc8(&buf[3], 2) != buf[5])
    {
        return SHTC3_ERR_CRC;
    }

    uint16_t raw_t = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    uint16_t raw_h = (uint16_t)((uint16_t)buf[3] << 8 | buf[4]);

    out->temperature_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    out->humidity_pct  = 100.0f * (float)raw_h / 65535.0f;

    /* Sleep after read */
    if (!send_cmd(dev, CMD_SLEEP_MSB, CMD_SLEEP_LSB))
    {
        return SHTC3_ERR_I2C;
    }

    ENSURE(out->temperature_c == out->temperature_c);   /* not NaN */
    ENSURE(out->humidity_pct == out->humidity_pct);       /* not NaN */
    return SHTC3_OK;
}

shtc3_err_t shtc3_sleep(shtc3_t *dev)
{
    if (!dev)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_SLEEP_MSB, CMD_SLEEP_LSB))
    {
        return SHTC3_ERR_I2C;
    }
    return SHTC3_OK;
}

shtc3_err_t shtc3_wakeup(shtc3_t *dev)
{
    if (!dev)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_WAKEUP_MSB, CMD_WAKEUP_LSB))
    {
        return SHTC3_ERR_I2C;
    }
    return SHTC3_OK;
}

shtc3_err_t shtc3_reset(shtc3_t *dev)
{
    if (!dev)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!dev->i2c)
    {
        return SHTC3_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_RESET_MSB, CMD_RESET_LSB))
    {
        return SHTC3_ERR_I2C;
    }
    return SHTC3_OK;
}
