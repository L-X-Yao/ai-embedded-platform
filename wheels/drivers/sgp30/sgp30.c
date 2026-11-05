// SPDX-License-Identifier: MIT
/*
 * sgp30 — Sensirion SGP30 air quality sensor driver (TVOC + eCO2)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 */

#include "sgp30.h"
#include "contract.h"
#include <string.h>

/* ---------- SGP30 command words (MSB first) ---------- */

#define CMD_IAQ_INIT_MSB      0x20u
#define CMD_IAQ_INIT_LSB      0x03u

#define CMD_MEASURE_IAQ_MSB   0x20u
#define CMD_MEASURE_IAQ_LSB   0x08u

#define CMD_GET_BASELINE_MSB  0x20u
#define CMD_GET_BASELINE_LSB  0x15u

#define CMD_SET_BASELINE_MSB  0x20u
#define CMD_SET_BASELINE_LSB  0x1Eu

#define CMD_GET_SERIAL_MSB    0x36u
#define CMD_GET_SERIAL_LSB    0x82u

#define CMD_SET_HUMIDITY_MSB  0x20u
#define CMD_SET_HUMIDITY_LSB  0x61u

/* ---------- CRC-8 (Sensirion: poly 0x31, init 0xFF) ---------- */

uint8_t sgp30_crc8(const uint8_t *data, uint16_t len)
{
    if (!data) {
        return 0;
    }
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

static bool send_cmd(const sgp30_t *dev, uint8_t msb, uint8_t lsb)
{
    uint8_t cmd[2] = { msb, lsb };
    return dev->i2c->write(dev->i2c->ctx, SGP30_DEV_ADDR, cmd, 2);
}

/**
 * Send command + data payload with CRC per word.
 * Each word is 2 data bytes + 1 CRC byte.
 */
static bool send_cmd_with_data(const sgp30_t *dev,
                               uint8_t cmd_msb, uint8_t cmd_lsb,
                               const uint8_t *words, uint16_t word_count)
{
    /* Max payload: 2 cmd + word_count * 3 (data_hi, data_lo, crc) */
    uint8_t buf[8]; /* 2 cmd + 2 words max = 2 + 6 = 8 */
    uint16_t idx = 0;

    buf[idx++] = cmd_msb;
    buf[idx++] = cmd_lsb;

    for (uint16_t w = 0; w < word_count; w++) {
        uint8_t hi = words[w * 2u];
        uint8_t lo = words[w * 2u + 1u];
        buf[idx++] = hi;
        buf[idx++] = lo;
        uint8_t pair[2] = { hi, lo };
        buf[idx++] = sgp30_crc8(pair, 2);
    }

    return dev->i2c->write(dev->i2c->ctx, SGP30_DEV_ADDR, buf, idx);
}

/**
 * Read response: word_count words, each 2 data bytes + 1 CRC byte.
 * Extracts validated data into out_data (2 * word_count bytes).
 * Returns SGP30_OK, SGP30_ERR_I2C, or SGP30_ERR_CRC.
 */
static sgp30_err_t read_words(const sgp30_t *dev,
                              uint8_t *out_data, uint16_t word_count)
{
    uint8_t buf[18]; /* max 6 words * 3 = 18 bytes */
    uint16_t read_len = (uint16_t)(word_count * 3u);

    if (!dev->i2c->read(dev->i2c->ctx, SGP30_DEV_ADDR, buf, read_len)) {
        return SGP30_ERR_I2C;
    }

    for (uint16_t w = 0; w < word_count; w++) {
        uint16_t base = (uint16_t)(w * 3u);
        uint8_t crc = sgp30_crc8(&buf[base], 2);
        if (crc != buf[base + 2u]) {
            return SGP30_ERR_CRC;
        }
        out_data[w * 2u]       = buf[base];
        out_data[w * 2u + 1u]  = buf[base + 1u];
    }

    return SGP30_OK;
}

/* ---------- public API ---------- */

sgp30_err_t sgp30_init(sgp30_t *dev, const sgp30_i2c_t *i2c)
{
    if (!dev || !i2c) {
        return SGP30_ERR_PARAM;
    }
    if (!i2c->write || !i2c->read || !i2c->delay_ms) {
        return SGP30_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* Send iaq_init (0x2003) — starts air quality measurement mode */
    if (!send_cmd(dev, CMD_IAQ_INIT_MSB, CMD_IAQ_INIT_LSB)) {
        return SGP30_ERR_I2C;
    }

    /* SGP30 needs 10ms after iaq_init before accepting commands */
    i2c->delay_ms(i2c->ctx, 10);

    return SGP30_OK;
}

sgp30_err_t sgp30_measure_iaq(sgp30_t *dev, sgp30_data_t *out)
{
    if (!dev || !out || !dev->i2c) {
        return SGP30_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_MEASURE_IAQ_MSB, CMD_MEASURE_IAQ_LSB)) {
        return SGP30_ERR_I2C;
    }

    /* SGP30 needs 12ms to complete measurement */
    dev->i2c->delay_ms(dev->i2c->ctx, 12);

    /* Response: 2 words (eCO2 first, then TVOC), each with CRC */
    uint8_t data[4];
    sgp30_err_t err = read_words(dev, data, 2);
    if (err != SGP30_OK) {
        return err;
    }

    out->eco2_ppm = (uint16_t)((uint16_t)data[0] << 8 | data[1]);
    out->tvoc_ppb = (uint16_t)((uint16_t)data[2] << 8 | data[3]);

    return SGP30_OK;
}

sgp30_err_t sgp30_get_baseline(sgp30_t *dev, sgp30_baseline_t *out)
{
    if (!dev || !out || !dev->i2c) {
        return SGP30_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_GET_BASELINE_MSB, CMD_GET_BASELINE_LSB)) {
        return SGP30_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 10);

    /* Response: 2 words (eCO2 baseline first, then TVOC baseline) */
    uint8_t data[4];
    sgp30_err_t err = read_words(dev, data, 2);
    if (err != SGP30_OK) {
        return err;
    }

    out->eco2_baseline = (uint16_t)((uint16_t)data[0] << 8 | data[1]);
    out->tvoc_baseline = (uint16_t)((uint16_t)data[2] << 8 | data[3]);

    return SGP30_OK;
}

sgp30_err_t sgp30_set_baseline(sgp30_t *dev, const sgp30_baseline_t *bl)
{
    if (!dev || !bl || !dev->i2c) {
        return SGP30_ERR_PARAM;
    }

    /* set_baseline expects TVOC first, then eCO2 (opposite of get!) */
    uint8_t words[4] = {
        (uint8_t)(bl->tvoc_baseline >> 8),
        (uint8_t)(bl->tvoc_baseline & 0xFFu),
        (uint8_t)(bl->eco2_baseline >> 8),
        (uint8_t)(bl->eco2_baseline & 0xFFu),
    };

    if (!send_cmd_with_data(dev, CMD_SET_BASELINE_MSB, CMD_SET_BASELINE_LSB,
                            words, 2)) {
        return SGP30_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 10);

    return SGP30_OK;
}

sgp30_err_t sgp30_get_serial_id(sgp30_t *dev, sgp30_serial_t *out)
{
    if (!dev || !out || !dev->i2c) {
        return SGP30_ERR_PARAM;
    }

    if (!send_cmd(dev, CMD_GET_SERIAL_MSB, CMD_GET_SERIAL_LSB)) {
        return SGP30_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 1);

    /* Response: 3 words = 6 bytes serial */
    sgp30_err_t err = read_words(dev, out->id, 3);
    if (err != SGP30_OK) {
        return err;
    }

    return SGP30_OK;
}

sgp30_err_t sgp30_set_humidity(sgp30_t *dev, uint16_t abs_humidity_8_8)
{
    if (!dev || !dev->i2c) {
        return SGP30_ERR_PARAM;
    }

    uint8_t words[2] = {
        (uint8_t)(abs_humidity_8_8 >> 8),
        (uint8_t)(abs_humidity_8_8 & 0xFFu),
    };

    if (!send_cmd_with_data(dev, CMD_SET_HUMIDITY_MSB, CMD_SET_HUMIDITY_LSB,
                            words, 1)) {
        return SGP30_ERR_I2C;
    }

    dev->i2c->delay_ms(dev->i2c->ctx, 10);

    return SGP30_OK;
}
