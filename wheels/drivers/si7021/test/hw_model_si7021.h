// SPDX-License-Identifier: MIT
/*
 * hw_model_si7021.h — Si7021 command-based hardware model
 *
 * The Si7021 is command-based, NOT register-based:
 * - Write 1-byte command, wait, read 2-3 byte response
 * - Commands: 0xF5 (RH no-hold), 0xF3 (temp no-hold), 0xE0 (read temp from RH),
 *   0xFE (reset), 0xE6/0xE7 (write/read user reg), 0xFA+0x0F / 0xFC+0xC9 (serial)
 *
 * This model maps commands to responses with proper CRC-8.
 * Does NOT use hw_model.h register infrastructure (command-response only).
 */

#ifndef HW_MODEL_SI7021_H
#define HW_MODEL_SI7021_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- Si7021 commands --- */
#define SI_CMD_MEASURE_RH_NOHOLD    0xF5u
#define SI_CMD_MEASURE_TEMP_NOHOLD  0xF3u
#define SI_CMD_READ_TEMP_FROM_RH    0xE0u
#define SI_CMD_RESET                0xFEu
#define SI_CMD_WRITE_USER_REG       0xE6u
#define SI_CMD_READ_USER_REG        0xE7u
#define SI_CMD_READ_ID1_A           0xFAu
#define SI_CMD_READ_ID1_B           0x0Fu
#define SI_CMD_READ_ID2_A           0xFCu
#define SI_CMD_READ_ID2_B           0xC9u

/* User register default (14-bit temp / 12-bit RH) */
#define SI_USER_REG_DEFAULT         0x3Au

/* Worst-case conversion times from Table 2 (p.5), rounded up to whole ms.
 * An RH measurement also measures temperature: 12 ms + 10.8 ms. */
#define SI_TCONV_RH_MS              23u
#define SI_TCONV_TEMP_MS            11u

/* --- Simulated environment --- */
typedef struct {
    float    true_temp_c;         /* simulated temperature */
    float    true_humidity_pct;   /* simulated humidity */
    uint8_t  user_reg;            /* user register value */
    uint32_t serial_a;            /* electronic serial number part A */
    uint32_t serial_b;            /* electronic serial number part B */
    uint32_t now_ms;              /* model time, advanced by delay_ms */
    uint32_t convert_done_ms;     /* reads are NACKed until this time */
    /* Command state */
    uint8_t  last_cmd;
    uint8_t  last_cmd2;           /* second byte of 2-byte commands */
    bool     two_byte_cmd;        /* waiting for second byte */
    uint16_t last_rh_raw;         /* raw RH from last measurement (for read_temp_from_rh) */
    uint16_t last_temp_raw;       /* raw temp from last RH measurement */
    int      measure_rh_count;
    int      measure_temp_count;
    int      reset_count;
    /* Response state */
    enum {
        SI_RESP_NONE = 0,
        SI_RESP_RH,
        SI_RESP_TEMP,
        SI_RESP_TEMP_FROM_RH,
        SI_RESP_USER_REG,
        SI_RESP_ID1,
        SI_RESP_ID2,
    } pending_response;
} si7021_model_t;

/* Si7021 CRC-8: poly 0x131 (x^8 + x^5 + x^4 + 1 = 0x31), init 0x00 */
static uint8_t si7021_model_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Convert humidity % to raw: raw = (RH + 6) / 125 * 65536 */
static uint16_t si7021_model_hum_to_raw(float rh)
{
    float norm = (rh + 6.0f) / 125.0f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65536.0f);
}

/* Convert temperature C to raw: raw = (T + 46.85) / 175.72 * 65536 */
static uint16_t si7021_model_temp_to_raw(float temp_c)
{
    float norm = (temp_c + 46.85f) / 175.72f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)(norm * 65536.0f);
}

/* --- I2C HAL callbacks --- */

__attribute__((unused))
static bool si7021_model_write(void *ctx, uint8_t dev_addr,
                               const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    si7021_model_t *m = (si7021_model_t *)ctx;

    if (len == 0) return true;

    uint8_t cmd = data[0];

    /* Handle 2-byte commands */
    if (len >= 2) {
        m->last_cmd = cmd;
        m->last_cmd2 = data[1];

        if (cmd == SI_CMD_WRITE_USER_REG) {
            m->user_reg = data[1];
            m->pending_response = SI_RESP_NONE;
            return true;
        }
        if (cmd == SI_CMD_READ_ID1_A && data[1] == SI_CMD_READ_ID1_B) {
            m->pending_response = SI_RESP_ID1;
            return true;
        }
        if (cmd == SI_CMD_READ_ID2_A && data[1] == SI_CMD_READ_ID2_B) {
            m->pending_response = SI_RESP_ID2;
            return true;
        }
    }

    m->last_cmd = cmd;

    switch (cmd) {
    case SI_CMD_MEASURE_RH_NOHOLD:
        m->last_rh_raw = si7021_model_hum_to_raw(m->true_humidity_pct);
        m->last_temp_raw = si7021_model_temp_to_raw(m->true_temp_c);
        m->pending_response = SI_RESP_RH;
        m->convert_done_ms = m->now_ms + SI_TCONV_RH_MS;
        m->measure_rh_count++;
        break;
    case SI_CMD_MEASURE_TEMP_NOHOLD:
        m->pending_response = SI_RESP_TEMP;
        m->convert_done_ms = m->now_ms + SI_TCONV_TEMP_MS;
        m->measure_temp_count++;
        break;
    case SI_CMD_READ_TEMP_FROM_RH:
        m->pending_response = SI_RESP_TEMP_FROM_RH;
        break;
    case SI_CMD_READ_USER_REG:
        m->pending_response = SI_RESP_USER_REG;
        break;
    case SI_CMD_RESET:
        m->user_reg = SI_USER_REG_DEFAULT;
        m->reset_count++;
        m->pending_response = SI_RESP_NONE;
        break;
    default:
        m->pending_response = SI_RESP_NONE;
        break;
    }

    return true;
}

__attribute__((unused))
static bool si7021_model_read(void *ctx, uint8_t dev_addr,
                              uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    si7021_model_t *m = (si7021_model_t *)ctx;

    /* p.20: "Device will NACK the slave address byte until conversion is
     * complete." */
    if (m->now_ms < m->convert_done_ms) {
        return false;
    }

    memset(data, 0xFF, len);

    switch (m->pending_response) {
    case SI_RESP_RH:
        if (len >= 3) {
            data[0] = (uint8_t)(m->last_rh_raw >> 8);
            data[1] = (uint8_t)(m->last_rh_raw & 0xFF);
            data[2] = si7021_model_crc8(data, 2);
        }
        break;

    case SI_RESP_TEMP:
    {
        uint16_t t_raw = si7021_model_temp_to_raw(m->true_temp_c);
        if (len >= 3) {
            data[0] = (uint8_t)(t_raw >> 8);
            data[1] = (uint8_t)(t_raw & 0xFF);
            data[2] = si7021_model_crc8(data, 2);
        }
        break;
    }

    case SI_RESP_TEMP_FROM_RH:
        /* Returns temp from previous RH measurement, no CRC */
        if (len >= 2) {
            data[0] = (uint8_t)(m->last_temp_raw >> 8);
            data[1] = (uint8_t)(m->last_temp_raw & 0xFF);
        }
        break;

    case SI_RESP_USER_REG:
        if (len >= 1) {
            data[0] = m->user_reg;
        }
        break;

    case SI_RESP_ID1:
        /* 8 bytes: SNA3, CRC, SNA2, CRC, SNA1, CRC, SNA0, CRC */
        if (len >= 8) {
            uint8_t sna[4] = {
                (uint8_t)(m->serial_a >> 24),
                (uint8_t)(m->serial_a >> 16),
                (uint8_t)(m->serial_a >> 8),
                (uint8_t)(m->serial_a & 0xFF),
            };
            data[0] = sna[0]; data[1] = si7021_model_crc8(&sna[0], 1);
            data[2] = sna[1]; data[3] = si7021_model_crc8(&sna[1], 1);
            data[4] = sna[2]; data[5] = si7021_model_crc8(&sna[2], 1);
            data[6] = sna[3]; data[7] = si7021_model_crc8(&sna[3], 1);
        }
        break;

    case SI_RESP_ID2:
        /* 6 bytes: SNB3, SNB2, CRC, SNB1, SNB0, CRC */
        if (len >= 6) {
            uint8_t snb[4] = {
                (uint8_t)(m->serial_b >> 24),
                (uint8_t)(m->serial_b >> 16),
                (uint8_t)(m->serial_b >> 8),
                (uint8_t)(m->serial_b & 0xFF),
            };
            data[0] = snb[0]; data[1] = snb[1];
            data[2] = si7021_model_crc8(&snb[0], 2);
            data[3] = snb[2]; data[4] = snb[3];
            data[5] = si7021_model_crc8(&snb[2], 2);
        }
        break;

    case SI_RESP_NONE:
    default:
        break;
    }

    m->pending_response = SI_RESP_NONE;
    return true;
}

/* Delay callback: the only source of time in the model */
__attribute__((unused))
static void si7021_model_delay(void *ctx, uint32_t ms)
{
    si7021_model_t *m = (si7021_model_t *)ctx;
    m->now_ms += ms;
}

static void si7021_model_init(si7021_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Default environment: 25C, 50% RH */
    m->true_temp_c = 25.0f;
    m->true_humidity_pct = 50.0f;
    m->user_reg = SI_USER_REG_DEFAULT;
    m->serial_a = 0x12345678;
    m->serial_b = 0xABCDEF01;
}

/* Helper: set simulated temperature */
static void si7021_model_set_temperature(si7021_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

/* Helper: set simulated humidity */
static void si7021_model_set_humidity(si7021_model_t *m, float humidity_pct)
{
    m->true_humidity_pct = humidity_pct;
}

/* Helper: set serial number */
static void si7021_model_set_serial(si7021_model_t *m, uint32_t a, uint32_t b)
{
    m->serial_a = a;
    m->serial_b = b;
}

#endif /* HW_MODEL_SI7021_H */
