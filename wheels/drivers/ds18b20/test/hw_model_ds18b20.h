// SPDX-License-Identifier: MIT
/*
 * hw_model_ds18b20.h — 1-Wire protocol model for DS18B20 temperature sensor
 *
 * NOT a register model. Simulates a DS18B20 device on a 1-Wire bus.
 * The driver calls HAL functions (reset, write_byte, read_byte) and
 * the model responds based on the command sequence:
 *
 *   0xCC (Skip ROM) + 0x44 (Convert T) -> start conversion
 *   0xCC (Skip ROM) + 0xBE (Read Scratchpad) -> return 9-byte scratchpad
 *   0x33 (Read ROM) -> return 8-byte ROM code
 *
 * The model computes correct scratchpad bytes + CRC8 for a configured
 * temperature value.
 */

#ifndef HW_MODEL_DS18B20_H
#define HW_MODEL_DS18B20_H

#include "ds18b20.h"
#include <string.h>

typedef enum {
    DS18B20_MODEL_STATE_IDLE,
    DS18B20_MODEL_STATE_ADDRESSED,
    DS18B20_MODEL_STATE_READ_ROM,
    DS18B20_MODEL_STATE_READ_SCRATCHPAD,
    DS18B20_MODEL_STATE_CONVERTING,
} ds18b20_model_state_t;

typedef struct {
    /* simulated device state */
    float   temperature_c;
    uint8_t rom[8];
    uint8_t scratchpad[9];
    int     present;           /* device on bus? */

    /* protocol state machine */
    ds18b20_model_state_t state;
    int     read_idx;

    /* diagnostics */
    int     reset_count;
    int     convert_count;
    int     read_scratchpad_count;
    uint8_t last_command;
} ds18b20_model_t;

static void ds18b20_model_update_scratchpad(ds18b20_model_t *m);

/*
 * The chip's own CRC, from the manual and not from the driver, so that a
 * wrong driver CRC fails here. DS18B20 REV 042208 p.8: CRC = X8 + X5 + X4 + 1,
 * shift register bits initialized to 0, least significant bit shifted in first.
 */
static uint8_t ds18b20_model_crc8(const uint8_t *data, size_t len)
{
    uint8_t sr = 0;
    for (size_t i = 0; i < len; i++) {
        for (unsigned bit = 0; bit < 8u; bit++) {
            unsigned in = ((unsigned)data[i] >> bit) & 1u;
            unsigned feedback = (sr ^ in) & 1u;
            sr = (uint8_t)(sr >> 1);
            if (feedback)
                sr = (uint8_t)(sr ^ 0x8Cu);
        }
    }
    return sr;
}

static void ds18b20_model_init(ds18b20_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->present = 1;
    /* DS18B20 family code 0x28 */
    m->rom[0] = 0x28;
    m->rom[1] = 0x01;
    m->rom[2] = 0x02;
    m->rom[3] = 0x03;
    m->rom[4] = 0x04;
    m->rom[5] = 0x05;
    m->rom[6] = 0x06;
    m->rom[7] = ds18b20_model_crc8(m->rom, 7);
    /* default power-on temperature: 85.0 C */
    m->temperature_c = 85.0f;
    ds18b20_model_update_scratchpad(m);
}

static void ds18b20_model_set_temperature(ds18b20_model_t *m, float temp_c)
{
    m->temperature_c = temp_c;
    ds18b20_model_update_scratchpad(m);
}

static void ds18b20_model_set_present(ds18b20_model_t *m, int present)
{
    m->present = present;
}

/*
 * Build 9-byte scratchpad from configured temperature.
 * DS18B20 scratchpad layout:
 *   [0] Temp LSB
 *   [1] Temp MSB
 *   [2] TH register (user byte 1)
 *   [3] TL register (user byte 2)
 *   [4] Config register (resolution)
 *   [5] Reserved (0xFF)
 *   [6] Reserved
 *   [7] Reserved (0x10)
 *   [8] CRC8 of bytes 0-7
 */
static void ds18b20_model_update_scratchpad(ds18b20_model_t *m)
{
    /* Convert temperature to raw 16-bit value (12-bit resolution) */
    int16_t raw = (int16_t)(m->temperature_c * 16.0f);
    m->scratchpad[0] = (uint8_t)(raw & 0xFF);
    m->scratchpad[1] = (uint8_t)((uint16_t)raw >> 8);
    m->scratchpad[2] = 0x00; /* TH */
    m->scratchpad[3] = 0x00; /* TL */
    m->scratchpad[4] = 0x7F; /* config: 12-bit resolution */
    m->scratchpad[5] = 0xFF; /* reserved */
    m->scratchpad[6] = 0x00; /* reserved */
    m->scratchpad[7] = 0x10; /* reserved */
    m->scratchpad[8] = ds18b20_model_crc8(m->scratchpad, 8);
}

/* --- HAL callbacks wired to model --- */

static bool ds18b20_model_reset(void *ctx)
{
    ds18b20_model_t *m = (ds18b20_model_t *)ctx;
    m->reset_count++;
    m->state = DS18B20_MODEL_STATE_IDLE;
    m->read_idx = 0;
    return m->present ? true : false;
}

static bool ds18b20_model_write_byte(void *ctx, uint8_t byte)
{
    ds18b20_model_t *m = (ds18b20_model_t *)ctx;
    m->last_command = byte;

    switch (m->state) {
    case DS18B20_MODEL_STATE_IDLE:
        /* expecting ROM command */
        if (byte == 0xCC) {
            /* Skip ROM */
            m->state = DS18B20_MODEL_STATE_ADDRESSED;
        } else if (byte == 0x55) {
            /* Match ROM -- for simplicity, always match */
            m->state = DS18B20_MODEL_STATE_ADDRESSED;
        } else if (byte == 0x33) {
            /* Read ROM */
            m->state = DS18B20_MODEL_STATE_READ_ROM;
            m->read_idx = 0;
        }
        break;

    case DS18B20_MODEL_STATE_ADDRESSED:
        /* expecting function command */
        if (byte == 0x44) {
            /* Convert T */
            m->convert_count++;
            m->state = DS18B20_MODEL_STATE_CONVERTING;
        } else if (byte == 0xBE) {
            /* Read Scratchpad */
            m->read_scratchpad_count++;
            m->state = DS18B20_MODEL_STATE_READ_SCRATCHPAD;
            m->read_idx = 0;
        } else if (byte == 0x4E) {
            /* Write Scratchpad -- consume 3 data bytes */
            /* simplified: just stay in addressed state */
        }
        break;

    case DS18B20_MODEL_STATE_READ_ROM:
    case DS18B20_MODEL_STATE_READ_SCRATCHPAD:
    case DS18B20_MODEL_STATE_CONVERTING:
    default:
        break;
    }

    return true;
}

static bool ds18b20_model_read_byte(void *ctx, uint8_t *byte)
{
    ds18b20_model_t *m = (ds18b20_model_t *)ctx;

    switch (m->state) {
    case DS18B20_MODEL_STATE_READ_ROM:
        if (m->read_idx < 8) {
            *byte = m->rom[m->read_idx++];
        } else {
            *byte = 0xFF;
        }
        break;

    case DS18B20_MODEL_STATE_READ_SCRATCHPAD:
        if (m->read_idx < 9) {
            *byte = m->scratchpad[m->read_idx++];
        } else {
            *byte = 0xFF;
        }
        break;

    case DS18B20_MODEL_STATE_IDLE:
    case DS18B20_MODEL_STATE_ADDRESSED:
    case DS18B20_MODEL_STATE_CONVERTING:
    default:
        *byte = 0xFF;
        break;
    }

    return true;
}

static ds18b20_ow_t ds18b20_model_ow(ds18b20_model_t *m)
{
    ds18b20_ow_t ow;
    ow.reset      = ds18b20_model_reset;
    ow.write_byte = ds18b20_model_write_byte;
    ow.read_byte  = ds18b20_model_read_byte;
    ow.ctx        = m;
    return ow;
}

/* --- Query API --- */

static int ds18b20_model_get_reset_count(const ds18b20_model_t *m)
{
    return m->reset_count;
}

static int ds18b20_model_get_convert_count(const ds18b20_model_t *m)
{
    return m->convert_count;
}

static int ds18b20_model_get_read_scratchpad_count(const ds18b20_model_t *m)
{
    return m->read_scratchpad_count;
}

static uint8_t ds18b20_model_get_scratchpad_crc(const ds18b20_model_t *m)
{
    return m->scratchpad[8];
}

static uint8_t ds18b20_model_verify_crc(const ds18b20_model_t *m)
{
    return ds18b20_crc8(m->scratchpad, 8);
}

#endif /* HW_MODEL_DS18B20_H */
