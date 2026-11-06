// SPDX-License-Identifier: MIT
/*
 * sgp40 — Sensirion SGP40 VOC raw signal sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 *
 * CRC-8 and command protocol from SGP40 datasheet (Sensirion).
 */

#include "sgp40.h"
#include "contract.h"
#include <string.h>

/* CRC-8 polynomial and init for Sensirion sensors */
#define CRC_POLY   0x31
#define CRC_INIT   0xFF

/* Self-test passed: first byte 0xD4, the second byte is undefined */
#define SELFTEST_PASSED  0xD4u

/* Sending both of these keeps on-chip compensation disabled */
#define DEFAULT_HUMIDITY    0x8000u  /* 50% RH */
#define DEFAULT_TEMPERATURE 0x6666u  /* 25 C */

uint8_t sgp40_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = CRC_INIT;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
            {
                crc = (uint8_t)((crc << 1) ^ CRC_POLY);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

static bool i2c_write(const sgp40_t *dev, const uint8_t *data, uint16_t len)
{
    return dev->i2c->write(dev->i2c->ctx, SGP40_DEV_ADDR, data, len);
}

static bool i2c_read(const sgp40_t *dev, uint8_t *data, uint16_t len)
{
    return dev->i2c->read(dev->i2c->ctx, SGP40_DEV_ADDR, data, len);
}

static void delay(const sgp40_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* The steps of init that talk to the chip */
static sgp40_err_t run_self_test(const sgp40_t *dev)
{
    /* Send self-test command: 0x280E */
    uint8_t cmd[2] = { 0x28, 0x0E };
    if (!i2c_write(dev, cmd, 2))
    {
        return SGP40_ERR_I2C;
    }

    delay(dev, 320);

    /* Read 3 bytes: MSB, LSB, CRC */
    uint8_t buf[3];
    if (!i2c_read(dev, buf, 3))
    {
        return SGP40_ERR_I2C;
    }

    /* Verify CRC */
    uint8_t crc = sgp40_crc8(buf, 2);
    if (crc != buf[2])
    {
        return SGP40_ERR_CRC;
    }

    /* Check self-test result */
    if (buf[0] != SELFTEST_PASSED)
    {
        return SGP40_ERR_SELFTEST;
    }

    return SGP40_OK;
}

sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c)
{
    if (!dev)
    {
        return SGP40_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the measure calls refuse a
     * device without it. delay_ms is optional: delay() tests it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !i2c->write || !i2c->read)
    {
        return SGP40_ERR_PARAM;
    }

    dev->i2c = i2c;

    sgp40_err_t err = run_self_test(dev);
    if (err != SGP40_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return SGP40_OK;
}

sgp40_err_t sgp40_measure_raw_comp(sgp40_t *dev, sgp40_data_t *out,
                                   uint16_t humidity, uint16_t temperature)
{
    if (!dev || !dev->i2c || !out)
    {
        return SGP40_ERR_PARAM;
    }

    /* Build command: 0x260F + humidity word + CRC + temperature word + CRC */
    uint8_t cmd[8];
    cmd[0] = 0x26;
    cmd[1] = 0x0F;

    /* Humidity word + CRC */
    cmd[2] = (uint8_t)(humidity >> 8);
    cmd[3] = (uint8_t)(humidity & 0xFF);
    cmd[4] = sgp40_crc8(&cmd[2], 2);

    /* Temperature word + CRC */
    cmd[5] = (uint8_t)(temperature >> 8);
    cmd[6] = (uint8_t)(temperature & 0xFF);
    cmd[7] = sgp40_crc8(&cmd[5], 2);

    if (!i2c_write(dev, cmd, 8))
    {
        return SGP40_ERR_I2C;
    }

    delay(dev, 30);

    /* Read 3 bytes: SRAW_MSB, SRAW_LSB, CRC */
    uint8_t buf[3];
    if (!i2c_read(dev, buf, 3))
    {
        return SGP40_ERR_I2C;
    }

    /* Verify CRC */
    uint8_t crc = sgp40_crc8(buf, 2);
    if (crc != buf[2])
    {
        return SGP40_ERR_CRC;
    }

    out->sraw_voc = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return SGP40_OK;
}

sgp40_err_t sgp40_measure_raw(sgp40_t *dev, sgp40_data_t *out)
{
    return sgp40_measure_raw_comp(dev, out,
                                 DEFAULT_HUMIDITY, DEFAULT_TEMPERATURE);
}
