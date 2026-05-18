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

/* ---------- model state ---------- */

typedef struct {
    uint8_t  last_cmd;
    uint16_t temperature_raw;
    uint16_t humidity_raw;
    uint32_t serial;
    bool     reset_done;
    int      write_count;
    int      read_count;
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

    m->last_cmd = data[0];

    if (data[0] == SHT40_CMD_SOFT_RESET) {
        m->reset_done = true;
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

/* ---------- delay callback (no-op) ---------- */

__attribute__((unused))
static void sht40_model_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_SHT40_H */
