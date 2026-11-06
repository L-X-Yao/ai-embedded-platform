// SPDX-License-Identifier: MIT
/*
 * scd40 — Sensirion SCD40/SCD41 NDIR CO2 + temperature + humidity sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "scd40.h"
#include "contract.h"
#include <string.h>

/* ---------- command codes (16-bit) ---------- */

#define CMD_START_PERIODIC_MSB  0x21u
#define CMD_START_PERIODIC_LSB  0xB1u
#define CMD_READ_MEASUREMENT_MSB 0xECu
#define CMD_READ_MEASUREMENT_LSB 0x05u
#define CMD_STOP_PERIODIC_MSB   0x3Fu
#define CMD_STOP_PERIODIC_LSB   0x86u
#define CMD_GET_SERIAL_MSB      0x36u
#define CMD_GET_SERIAL_LSB      0x82u
#define CMD_REINIT_MSB          0x36u
#define CMD_REINIT_LSB          0x46u
#define CMD_SET_ALTITUDE_MSB    0x24u
#define CMD_SET_ALTITUDE_LSB    0x27u
#define CMD_GET_DATA_READY_MSB  0xE4u
#define CMD_GET_DATA_READY_LSB  0xB8u

/* ---------- command execution times, ms (Table 9) ---------- */

#define STOP_PERIODIC_TIME_MS   500u
#define REINIT_TIME_MS          30u
#define SET_ALTITUDE_TIME_MS    1u

/* ---------- internal helpers ---------- */

uint8_t scd40_crc8(const uint8_t *data, uint16_t len)
{
    if (!data) { return 0xFF; }
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80u)
            {
                crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x31u);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/** Send a bare 2-byte command (no payload). */
static bool send_cmd(const scd40_t *dev, uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return dev->i2c->write(dev->i2c->ctx, dev->addr, cmd, 2);
}

/** Send a 2-byte command + 2-byte argument + CRC (5 bytes total). */
static bool send_cmd_arg(const scd40_t *dev, uint8_t msb, uint8_t lsb,
                         uint16_t arg)
{
    uint8_t buf[5];
    buf[0] = msb;
    buf[1] = lsb;
    buf[2] = (uint8_t)(arg >> 8);
    buf[3] = (uint8_t)(arg & 0xFFu);
    buf[4] = scd40_crc8(&buf[2], 2);
    return dev->i2c->write(dev->i2c->ctx, dev->addr, buf, 5);
}

/**
 * Read N words (each word = 2 data bytes + 1 CRC byte = 3 bytes).
 * Returns decoded uint16 values in out[0..n_words-1].
 * Verifies CRC-8 on each word.
 */
static scd40_err_t read_words(const scd40_t *dev, uint16_t *out,
                              uint16_t n_words)
{
    uint16_t n_bytes = (uint16_t)(n_words * 3u);
    /* Stack buffer for up to 3 words (9 bytes) — covers all SCD40 reads. */
    uint8_t buf[9];

    if (n_bytes > sizeof(buf))
    {
        return SCD40_ERR_PARAM;
    }

    if (!dev->i2c->read(dev->i2c->ctx, dev->addr, buf, n_bytes))
    {
        return SCD40_ERR_I2C;
    }

    for (uint16_t i = 0; i < n_words; i++)
    {
        uint16_t off = (uint16_t)(i * 3u);
        if (scd40_crc8(&buf[off], 2) != buf[off + 2u])
        {
            return SCD40_ERR_CRC;
        }
        out[i] = (uint16_t)((uint16_t)buf[off] << 8 | buf[off + 1u]);
    }

    return SCD40_OK;
}

/* ---------- public API ---------- */

/* Sensirion SCD4x datasheet p.9: "While a periodic measurement is running, no
 * other commands must be issued with the exception of read_measurement,
 * get_data_ready_status, stop_periodic_measurement, set_ambient_pressure and
 * get_ambient_pressure." Table 9 marks the others "no" under During meas. */

scd40_err_t scd40_init(scd40_t *dev, const scd40_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return SCD40_ERR_PARAM;
    }
    if (!i2c->write || !i2c->read || !i2c->delay_ms)
    {
        return SCD40_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c  = i2c;
    dev->addr = SCD40_DEV_ADDR;

    return SCD40_OK;
}

scd40_err_t scd40_start_periodic(scd40_t *dev)
{
    if (!dev)
    {
        return SCD40_ERR_PARAM;
    }
    if (dev->periodic)
    {
        return SCD40_ERR_BUSY;
    }
    if (!send_cmd(dev, CMD_START_PERIODIC_MSB, CMD_START_PERIODIC_LSB))
    {
        return SCD40_ERR_I2C;
    }
    dev->periodic = true;
    return SCD40_OK;
}

scd40_err_t scd40_stop_periodic(scd40_t *dev)
{
    if (!dev)
    {
        return SCD40_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_STOP_PERIODIC_MSB, CMD_STOP_PERIODIC_LSB))
    {
        return SCD40_ERR_I2C;
    }
    dev->periodic = false;
    dev->i2c->delay_ms(dev->i2c->ctx, STOP_PERIODIC_TIME_MS);
    return SCD40_OK;
}

scd40_err_t scd40_read(scd40_t *dev, scd40_data_t *out)
{
    if (!dev || !out)
    {
        return SCD40_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_READ_MEASUREMENT_MSB, CMD_READ_MEASUREMENT_LSB))
    {
        return SCD40_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 1);

    uint16_t words[3];
    scd40_err_t err = read_words(dev, words, 3);
    if (err != SCD40_OK)
    {
        return err;
    }

    out->co2_ppm       = words[0];
    out->temperature_c = -45.0f + 175.0f * (float)words[1] / 65535.0f;
    out->humidity_pct  = 100.0f * (float)words[2] / 65535.0f;

    ENSURE(out->temperature_c == out->temperature_c);   /* not NaN */
    ENSURE(out->humidity_pct == out->humidity_pct);       /* not NaN */
    return SCD40_OK;
}

scd40_err_t scd40_data_ready(scd40_t *dev, bool *ready)
{
    if (!dev || !ready)
    {
        return SCD40_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_GET_DATA_READY_MSB, CMD_GET_DATA_READY_LSB))
    {
        return SCD40_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 1);

    uint16_t status;
    scd40_err_t err = read_words(dev, &status, 1);
    if (err != SCD40_OK)
    {
        return err;
    }

    /* Data is ready when any of the low 11 bits is set. Bits [15:11] carry
     * no meaning: the manual's not-ready example is 0x8000 (Table 23). */
    *ready = (status & 0x07FFu) != 0u;

    return SCD40_OK;
}

scd40_err_t scd40_get_serial(scd40_t *dev, uint16_t serial[3])
{
    if (!dev || !serial)
    {
        return SCD40_ERR_PARAM;
    }
    if (dev->periodic)
    {
        return SCD40_ERR_BUSY;
    }

    if (!send_cmd(dev, CMD_GET_SERIAL_MSB, CMD_GET_SERIAL_LSB))
    {
        return SCD40_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 1);

    return read_words(dev, serial, 3);
}

scd40_err_t scd40_set_altitude(scd40_t *dev, uint16_t meters)
{
    if (!dev || meters > SCD40_ALTITUDE_MAX_M)
    {
        return SCD40_ERR_PARAM;
    }
    if (dev->periodic)
    {
        return SCD40_ERR_BUSY;
    }
    if (!send_cmd_arg(dev, CMD_SET_ALTITUDE_MSB, CMD_SET_ALTITUDE_LSB, meters))
    {
        return SCD40_ERR_I2C;
    }
    dev->i2c->delay_ms(dev->i2c->ctx, SET_ALTITUDE_TIME_MS);
    return SCD40_OK;
}

scd40_err_t scd40_perform_reset(scd40_t *dev)
{
    if (!dev)
    {
        return SCD40_ERR_PARAM;
    }
    if (dev->periodic)
    {
        return SCD40_ERR_BUSY;
    }
    if (!send_cmd(dev, CMD_REINIT_MSB, CMD_REINIT_LSB))
    {
        return SCD40_ERR_I2C;
    }
    dev->i2c->delay_ms(dev->i2c->ctx, REINIT_TIME_MS);
    return SCD40_OK;
}
