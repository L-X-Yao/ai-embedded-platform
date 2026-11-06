// SPDX-License-Identifier: MIT
/*
 * hw_model_sht40.h — SHT40 hardware model from datasheet
 *
 * Models Sensirion SHT40 behavior:
 * - Command-response I2C protocol (not register-based)
 * - 1-byte command write, 6-byte response read
 * - Commands: 0xFD (high), 0xF6 (medium), 0xE0 (low), 0x94 (reset), 0x89 (serial)
 * - Response: 2 bytes data + CRC8 + 2 bytes data + CRC8
 * - CRC-8: polynomial 0x31, init 0xFF (Sensirion standard)
 * - Busy for the maximum of Table 5 after a measurement or soft reset
 *   command: read headers get a NACK and commands are lost. Time comes from
 *   the delay callback.
 * - Measurement data can be read once (p.11)
 * - The model starts idle: tPU after VDD is the system's to wait out.
 *
 * Internal state: temperature_raw and humidity_raw, computed from physical
 * values via inverse conversion formulas from the datasheet:
 *   raw_t = (temp_c + 45) * 65535 / 175
 *   raw_h = (hum_pct + 6) * 65535 / 125
 */

#ifndef HW_MODEL_SHT40_H
#define HW_MODEL_SHT40_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---------- SHT40 command bytes ---------- */

#define SHT40_CMD_MEASURE_HIGH   0xFDu
#define SHT40_CMD_MEASURE_MEDIUM 0xF6u
#define SHT40_CMD_MEASURE_LOW    0xE0u
#define SHT40_CMD_SOFT_RESET     0x94u
#define SHT40_CMD_READ_SERIAL    0x89u

/* ---------- p.10 Table 5, maximum values ---------- */

#define SHT40_T_SR_US      1000u
#define SHT40_T_MEAS_L_US  1600u
#define SHT40_T_MEAS_M_US  4500u
#define SHT40_T_MEAS_H_US  8300u

/* ---------- model state ---------- */

typedef struct {
    uint8_t  last_cmd;
    uint16_t temperature_raw;
    uint16_t humidity_raw;
    uint32_t serial;
    bool     reset_done;
    int      write_count;
    int      read_count;
    uint32_t now_us;      /* model clock, moved by the delay callback */
    uint32_t idle_us;     /* the sensor is busy until this time */
    bool     has_data;    /* a response is waiting for a read header */
    int      nack_count;  /* read headers refused */
} sht40_model_t;

/* ---------- CRC-8 (poly 0x31, init 0xFF) ---------- */

static uint8_t sht40_model_crc8(const uint8_t *data, uint16_t len)
{
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

/* ---------- init ---------- */

static void sht40_model_init(sht40_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->serial = 0x12345678u;
    /* Default: 25 C, 50% RH */
    /* raw_t = (25 + 45) * 65535 / 175 = 26214 */
    m->temperature_raw = 26214u;
    /* raw_h = (50 + 6) * 65535 / 125 = 29359 */
    m->humidity_raw = 29359u;
}

/* ---------- helpers: set physical values ---------- */

__attribute__((unused))
static void sht40_model_set_temperature_c(sht40_model_t *m, float temp_c)
{
    /* Inverse of: temp_c = -45 + 175 * raw / 65535 */
    float raw_f = (temp_c + 45.0f) * 65535.0f / 175.0f;
    if (raw_f < 0.0f) raw_f = 0.0f;
    if (raw_f > 65535.0f) raw_f = 65535.0f;
    m->temperature_raw = (uint16_t)(raw_f + 0.5f);
}

__attribute__((unused))
static void sht40_model_set_humidity_pct(sht40_model_t *m, float hum_pct)
{
    /* Inverse of: hum_pct = -6 + 125 * raw / 65535 */
    float raw_f = (hum_pct + 6.0f) * 65535.0f / 125.0f;
    if (raw_f < 0.0f) raw_f = 0.0f;
    if (raw_f > 65535.0f) raw_f = 65535.0f;
    m->humidity_raw = (uint16_t)(raw_f + 0.5f);
}

__attribute__((unused))
static void sht40_model_set_serial(sht40_model_t *m, uint32_t serial)
{
    m->serial = serial;
}

/* ---------- I2C write callback ---------- */

__attribute__((unused))
static bool sht40_model_write(void *ctx, uint8_t addr,
                              const uint8_t *data, uint16_t len)
{
    (void)addr;
    sht40_model_t *m = (sht40_model_t *)ctx;
    m->write_count++;

    if (len != 1) {
        return false;  /* SHT40 only accepts 1-byte commands */
    }

    /* Table 8: a sensor "not ready to process a command" answers the read
     * header with NACK, so a command sent while it is busy is lost. */
    if (m->now_us < m->idle_us) {
        return true;
    }

    m->last_cmd = data[0];
    m->has_data = false;

    switch (data[0]) {
    case SHT40_CMD_SOFT_RESET:
        m->reset_done = true;
        m->idle_us = m->now_us + SHT40_T_SR_US;
        break;
    case SHT40_CMD_MEASURE_HIGH:
        m->has_data = true;
        m->idle_us = m->now_us + SHT40_T_MEAS_H_US;
        break;
    case SHT40_CMD_MEASURE_MEDIUM:
        m->has_data = true;
        m->idle_us = m->now_us + SHT40_T_MEAS_M_US;
        break;
    case SHT40_CMD_MEASURE_LOW:
        m->has_data = true;
        m->idle_us = m->now_us + SHT40_T_MEAS_L_US;
        break;
    case SHT40_CMD_READ_SERIAL:
        m->has_data = true;   /* Table 5 gives this command no duration */
        break;
    default:
        break;
    }

    return true;
}

/* ---------- I2C read callback ---------- */

__attribute__((unused))
static bool sht40_model_read(void *ctx, uint8_t addr,
                             uint8_t *data, uint16_t len)
{
    (void)addr;
    sht40_model_t *m = (sht40_model_t *)ctx;
    m->read_count++;

    if (len < 6) {
        return false;
    }

    /* p.10-11 "In case the sensor receives a read header and is still busy
     * with e.g. measurement or heating, it will return a NACK." */
    if (m->now_us < m->idle_us || !m->has_data) {
        m->nack_count++;
        return false;
    }

    switch (m->last_cmd) {
    case SHT40_CMD_MEASURE_HIGH:
    case SHT40_CMD_MEASURE_MEDIUM:
    case SHT40_CMD_MEASURE_LOW: {
        /* Temperature: 2 bytes big-endian + CRC */
        data[0] = (uint8_t)(m->temperature_raw >> 8);
        data[1] = (uint8_t)(m->temperature_raw & 0xFFu);
        data[2] = sht40_model_crc8(&data[0], 2);
        /* Humidity: 2 bytes big-endian + CRC */
        data[3] = (uint8_t)(m->humidity_raw >> 8);
        data[4] = (uint8_t)(m->humidity_raw & 0xFFu);
        data[5] = sht40_model_crc8(&data[3], 2);
        /* p.11 "Measurement data can only be received once" */
        m->has_data = false;
        break;
    }
    case SHT40_CMD_READ_SERIAL: {
        /* Serial: upper 16 bits + CRC, lower 16 bits + CRC */
        uint16_t word0 = (uint16_t)(m->serial >> 16);
        uint16_t word1 = (uint16_t)(m->serial & 0xFFFFu);
        data[0] = (uint8_t)(word0 >> 8);
        data[1] = (uint8_t)(word0 & 0xFFu);
        data[2] = sht40_model_crc8(&data[0], 2);
        data[3] = (uint8_t)(word1 >> 8);
        data[4] = (uint8_t)(word1 & 0xFFu);
        data[5] = sht40_model_crc8(&data[3], 2);
        break;
    }
    default:
        /* Unknown command — fill with zeros */
        memset(data, 0, len);
        break;
    }

    return true;
}

/* ---------- delay callback ---------- */

__attribute__((unused))
static void sht40_model_delay(void *ctx, uint16_t ms)
{
    sht40_model_t *m = (sht40_model_t *)ctx;
    m->now_us += (uint32_t)ms * 1000u;
}

#endif /* HW_MODEL_SHT40_H */
