// SPDX-License-Identifier: MIT
/*
 * hw_model_ds18b20.h — 1-Wire protocol model for DS18B20 temperature sensor
 *
 * NOT a register model. Simulates one externally powered DS18B20 on a 1-Wire
 * bus, following DS18B20 REV 042208. The driver calls the HAL functions
 * (reset, write_byte, read_byte) and the model answers as the manual says:
 *
 *   p.10  A transaction is reset, ROM command, function command. A device
 *         that sees a step missing or out of order stays silent until reset.
 *   p.10  After Match ROM only the exact 64-bit code gets an answer.
 *   p.11  The temperature register changes only when a conversion ends. Read
 *         slots give 0 until then and 1 afterwards.
 *   p.8   tCONV depends on the resolution. The model takes the maximum of
 *         Table 2, and time passes only through ds18b20_model_advance_us().
 *   p.3   Below 12 bits the low bits are undefined. The model sets them, so
 *         a driver that keeps them reads high.
 *   p.7   Write Scratchpad reaches bytes 2, 3 and 4 only, and at power-up
 *         those bytes come back from EEPROM.
 *   p.8   Bit 7 and bits 0 to 4 of the configuration register cannot be
 *         written.
 *   p.11  A reset inside Write Scratchpad may corrupt the data: it is counted.
 *
 * The model computes the CRC from the polynomial, not with the driver.
 */

#ifndef HW_MODEL_DS18B20_H
#define HW_MODEL_DS18B20_H

#include "ds18b20.h"
#include <string.h>

typedef enum {
    DS18B20_MODEL_STATE_SILENT,          /* waits for a reset pulse */
    DS18B20_MODEL_STATE_IDLE,            /* after presence: ROM command next */
    DS18B20_MODEL_STATE_MATCH_ROM,       /* takes the 8 ROM code bytes */
    DS18B20_MODEL_STATE_ADDRESSED,       /* function command next */
    DS18B20_MODEL_STATE_READ_ROM,
    DS18B20_MODEL_STATE_READ_SCRATCHPAD,
    DS18B20_MODEL_STATE_WRITE_SCRATCHPAD,
    DS18B20_MODEL_STATE_CONVERTING,      /* read slots give the status */
} ds18b20_model_state_t;

/* Table 2, p.8: 12 bits take tCONV, each bit less takes half of that. */
#define DS18B20_MODEL_TCONV_US  750000u

typedef struct {
    /* simulated device state */
    float   temperature_c;     /* what the sensor sees, not the register */
    uint8_t rom[8];
    uint8_t scratchpad[9];
    uint8_t eeprom[3];         /* TH, TL, configuration */
    int     present;           /* device on bus? */
    uint32_t conversion_left_us;   /* 0 when no conversion runs */

    /* protocol state machine */
    ds18b20_model_state_t state;
    int     read_idx;

    /* diagnostics */
    int     reset_count;
    int     convert_count;
    int     read_scratchpad_count;
    int     partial_write_count;
    uint8_t last_command;
} ds18b20_model_t;

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

static void ds18b20_model_update_crc(ds18b20_model_t *m)
{
    m->scratchpad[8] = ds18b20_model_crc8(m->scratchpad, 8);
}

/*
 * 9-byte scratchpad, Figure 7 p.7:
 *   [0] Temp LSB, power-up 50h
 *   [1] Temp MSB, power-up 05h (85 C)
 *   [2] TH register (user byte 1), from EEPROM
 *   [3] TL register (user byte 2), from EEPROM
 *   [4] Config register, from EEPROM
 *   [5] Reserved (FFh)
 *   [6] Reserved
 *   [7] Reserved (10h)
 *   [8] CRC8 of bytes 0-7
 */
static void ds18b20_model_power_cycle(ds18b20_model_t *m)
{
    m->scratchpad[0] = 0x50;
    m->scratchpad[1] = 0x05;
    memcpy(&m->scratchpad[2], m->eeprom, sizeof(m->eeprom));
    m->scratchpad[5] = 0xFF;
    m->scratchpad[6] = 0x00;
    m->scratchpad[7] = 0x10;
    ds18b20_model_update_crc(m);
    m->conversion_left_us = 0;
    m->state = DS18B20_MODEL_STATE_SILENT;
    m->read_idx = 0;
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
    /* TH and TL are the user's (p.7); the configuration default is 12 bits */
    m->eeprom[0] = 0x4B;
    m->eeprom[1] = 0x46;
    m->eeprom[2] = 0x7F;
    /* the sensor reads 85 C until a test sets something else */
    m->temperature_c = 85.0f;
    ds18b20_model_power_cycle(m);
}

/* Sets what the sensor sees. Only a finished Convert T reaches the register. */
static void ds18b20_model_set_temperature(ds18b20_model_t *m, float temp_c)
{
    m->temperature_c = temp_c;
}

static void ds18b20_model_set_present(ds18b20_model_t *m, int present)
{
    m->present = present;
}

/* R1 R0 of the configuration register: 0 for 9 bits to 3 for 12 bits. */
static unsigned ds18b20_model_resolution(const ds18b20_model_t *m)
{
    return ((unsigned)m->scratchpad[4] >> 5) & 3u;
}

static void ds18b20_model_end_conversion(ds18b20_model_t *m)
{
    int16_t raw = (int16_t)(m->temperature_c * 16.0f);
    uint16_t undefined = (uint16_t)(0x0007u >> ds18b20_model_resolution(m));
    uint16_t bits = (uint16_t)((uint16_t)raw | undefined);
    m->scratchpad[0] = (uint8_t)(bits & 0xFFu);
    m->scratchpad[1] = (uint8_t)(bits >> 8);
    ds18b20_model_update_crc(m);
}

/* Lets time pass. The model has no clock of its own. */
static void ds18b20_model_advance_us(ds18b20_model_t *m, uint32_t us)
{
    if (m->conversion_left_us == 0)
        return;
    if (us < m->conversion_left_us) {
        m->conversion_left_us -= us;
        return;
    }
    m->conversion_left_us = 0;
    ds18b20_model_end_conversion(m);
}

/* --- HAL callbacks wired to model --- */

static bool ds18b20_model_reset(void *ctx)
{
    ds18b20_model_t *m = (ds18b20_model_t *)ctx;
    m->reset_count++;
    if (m->state == DS18B20_MODEL_STATE_WRITE_SCRATCHPAD)
        m->partial_write_count++;
    /* a conversion goes on across a reset: only the transaction ends */
    m->state = m->present ? DS18B20_MODEL_STATE_IDLE
                          : DS18B20_MODEL_STATE_SILENT;
    m->read_idx = 0;
    return m->present ? true : false;
}

static void ds18b20_model_rom_command(ds18b20_model_t *m, uint8_t byte)
{
    m->read_idx = 0;
    if (byte == 0xCC)
        m->state = DS18B20_MODEL_STATE_ADDRESSED;
    else if (byte == 0x55)
        m->state = DS18B20_MODEL_STATE_MATCH_ROM;
    else if (byte == 0x33)
        m->state = DS18B20_MODEL_STATE_READ_ROM;
    else
        m->state = DS18B20_MODEL_STATE_SILENT;
}

static void ds18b20_model_function_command(ds18b20_model_t *m, uint8_t byte)
{
    m->read_idx = 0;
    if (byte == 0x44) {
        m->convert_count++;
        m->conversion_left_us = DS18B20_MODEL_TCONV_US
                                >> (3u - ds18b20_model_resolution(m));
        m->state = DS18B20_MODEL_STATE_CONVERTING;
    } else if (byte == 0xBE) {
        m->read_scratchpad_count++;
        m->state = DS18B20_MODEL_STATE_READ_SCRATCHPAD;
    } else if (byte == 0x4E) {
        m->state = DS18B20_MODEL_STATE_WRITE_SCRATCHPAD;
    } else {
        m->state = DS18B20_MODEL_STATE_SILENT;
    }
}

static void ds18b20_model_write_scratchpad(ds18b20_model_t *m, uint8_t byte)
{
    if (m->read_idx == 2) {
        /* only R1 and R0: Figure 8 has bit 7 at 0 and bits 4 to 0 at 1 */
        m->scratchpad[4] = (uint8_t)((byte & 0x60u) | 0x1Fu);
        m->state = DS18B20_MODEL_STATE_SILENT;
    } else {
        m->scratchpad[2 + m->read_idx] = byte;
    }
    m->read_idx++;
    ds18b20_model_update_crc(m);
}

static bool ds18b20_model_write_byte(void *ctx, uint8_t byte)
{
    ds18b20_model_t *m = (ds18b20_model_t *)ctx;
    m->last_command = byte;

    switch (m->state) {
    case DS18B20_MODEL_STATE_IDLE:
        ds18b20_model_rom_command(m, byte);
        break;

    case DS18B20_MODEL_STATE_MATCH_ROM:
        if (byte != m->rom[m->read_idx++])
            m->state = DS18B20_MODEL_STATE_SILENT;
        else if (m->read_idx == 8)
            m->state = DS18B20_MODEL_STATE_ADDRESSED;
        break;

    case DS18B20_MODEL_STATE_ADDRESSED:
        ds18b20_model_function_command(m, byte);
        break;

    case DS18B20_MODEL_STATE_WRITE_SCRATCHPAD:
        ds18b20_model_write_scratchpad(m, byte);
        break;

    case DS18B20_MODEL_STATE_SILENT:
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

    /* with nobody driving it the pullup holds the bus at 1 */
    *byte = 0xFF;

    switch (m->state) {
    case DS18B20_MODEL_STATE_READ_ROM:
        *byte = m->rom[m->read_idx++];
        if (m->read_idx == 8) {
            m->read_idx = 0;
            m->state = DS18B20_MODEL_STATE_ADDRESSED;
        }
        break;

    case DS18B20_MODEL_STATE_READ_SCRATCHPAD:
        if (m->read_idx < 9)
            *byte = m->scratchpad[m->read_idx++];
        break;

    case DS18B20_MODEL_STATE_CONVERTING:
        if (m->conversion_left_us != 0)
            *byte = 0x00;
        break;

    case DS18B20_MODEL_STATE_SILENT:
    case DS18B20_MODEL_STATE_IDLE:
    case DS18B20_MODEL_STATE_MATCH_ROM:
    case DS18B20_MODEL_STATE_ADDRESSED:
    case DS18B20_MODEL_STATE_WRITE_SCRATCHPAD:
    default:
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

static int ds18b20_model_get_partial_write_count(const ds18b20_model_t *m)
{
    return m->partial_write_count;
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
