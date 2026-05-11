// SPDX-License-Identifier: MIT
/*
 * sgp40 — Sensirion SGP40 VOC raw signal sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command protocol from SGP40 datasheet (Sensirion, Version 1.1).
 * CRC-8: polynomial 0x31, init 0xFF (same as SHT/SGP30 series).
 */

#include "sgp40.h"
#include "contract.h"
#include <string.h>

/* ---------- command words (from datasheet Table 10) ---------- */

#define CMD_MEASURE_RAW_MSB   0x26u
#define CMD_MEASURE_RAW_LSB   0x0Fu
#define CMD_SELF_TEST_MSB     0x28u
#define CMD_SELF_TEST_LSB     0x0Eu
#define CMD_HEATER_OFF_MSB    0x36u
#define CMD_HEATER_OFF_LSB    0x15u

/* Self-test expected result */
#define SELF_TEST_OK_MSB      0xD4u
#define SELF_TEST_OK_LSB      0x00u

/* ---------- CRC-8 (Sensirion: poly 0x31, init 0xFF) ---------- */

uint8_t sgp40_crc8(const uint8_t *data, uint16_t len)
{
    if (!data)
        return 0;

    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x31u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ---------- internal helpers ---------- */

static bool send_cmd(const sgp40_t *dev, uint8_t msb, uint8_t lsb)
{
    /* Use write_reg: reg=MSB, val=LSB to send 2-byte command */
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, msb, lsb);
}

static bool read_bytes(const sgp40_t *dev, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->addr, 0, buf, len);
}

static void delay(const sgp40_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms) {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* Read 2 data bytes + 1 CRC byte, verify CRC, return word */
static sgp40_err_t read_word_crc(const sgp40_t *dev, uint16_t *out)
{
    uint8_t buf[3];
    if (!read_bytes(dev, buf, 3))
        return SGP40_ERR_I2C;

    uint8_t expected_crc = sgp40_crc8(buf, 2);
    if (buf[2] != expected_crc)
        return SGP40_ERR_CRC;

    *out = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return SGP40_OK;
}

sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c)
{
    if (!dev || !i2c)
        return SGP40_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return SGP40_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c  = i2c;
    dev->addr = SGP40_DEV_ADDR;

    /* Run self-test */
    if (!send_cmd(dev, CMD_SELF_TEST_MSB, CMD_SELF_TEST_LSB))
        return SGP40_ERR_I2C;
    delay(dev, 320);  /* datasheet: 320ms max */

    uint16_t result = 0;
    sgp40_err_t err = read_word_crc(dev, &result);
    if (err != SGP40_OK)
        return err;

    if (result != 0xD400)
        return SGP40_ERR_SELF_TEST;

    INVARIANT(dev->i2c != NULL);
    return SGP40_OK;
}

sgp40_err_t sgp40_measure_raw(sgp40_t *dev, uint16_t rh_ticks,
                              uint16_t t_ticks, sgp40_data_t *out)
{
    if (!dev || !out)
        return SGP40_ERR_PARAM;

    /* Send measure_raw command with humidity and temperature parameters.
     * Command: 0x260F + rh_msb + rh_lsb + rh_crc + t_msb + t_lsb + t_crc
     * We send the command first, then write the parameter bytes. */
    if (!send_cmd(dev, CMD_MEASURE_RAW_MSB, CMD_MEASURE_RAW_LSB))
        return SGP40_ERR_I2C;

    /* Write humidity parameter (2 bytes + CRC) */
    uint8_t rh_buf[2] = {
        (uint8_t)(rh_ticks >> 8),
        (uint8_t)(rh_ticks & 0xFF)
    };
    uint8_t rh_crc = sgp40_crc8(rh_buf, 2);
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, rh_buf[0], rh_buf[1]))
        return SGP40_ERR_I2C;
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, rh_crc, 0))
        return SGP40_ERR_I2C;

    /* Write temperature parameter (2 bytes + CRC) */
    uint8_t t_buf[2] = {
        (uint8_t)(t_ticks >> 8),
        (uint8_t)(t_ticks & 0xFF)
    };
    uint8_t t_crc = sgp40_crc8(t_buf, 2);
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, t_buf[0], t_buf[1]))
        return SGP40_ERR_I2C;
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, t_crc, 0))
        return SGP40_ERR_I2C;

    delay(dev, 30);  /* datasheet: 30ms measurement time */

    /* Read SRAW (2 bytes + CRC) */
    uint16_t sraw = 0;
    sgp40_err_t err = read_word_crc(dev, &sraw);
    if (err != SGP40_OK)
        return err;

    out->sraw = sraw;
    return SGP40_OK;
}
