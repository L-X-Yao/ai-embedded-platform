// SPDX-License-Identifier: MIT
/*
 * hw_model_sgp30.h -- SGP30 command-based hardware model
 *
 * The SGP30 is NOT register-based -- it uses 16-bit commands over I2C:
 * - Write 2-byte command (MSB, LSB), optionally followed by data words+CRC
 * - Read response as word+CRC triplets (2 data bytes + 1 CRC byte per word)
 *
 * Supported commands:
 *   0x2003  iaq_init       -- start air quality measurement
 *   0x2008  measure_iaq    -- returns eCO2 (word 0) + TVOC (word 1)
 *   0x2015  get_baseline   -- returns eCO2_bl (word 0) + TVOC_bl (word 1)
 *   0x201E  set_baseline   -- receives TVOC_bl (word 0) + eCO2_bl (word 1)
 *   0x3682  get_serial     -- returns 3 words (48-bit serial)
 *   0x2061  set_humidity   -- receives 1 word (8.8 fixed-point g/m3)
 *
 * CRC-8: polynomial 0x31, init 0xFF (Sensirion standard).
 * Does NOT use hw_model.h register infrastructure (command-response only).
 *
 * After a command the sensor is busy for the maximum duration of Table 10, and
 * refuses reads and writes until the delay callback has let that time pass
 * (p.8). A response reads once, and every byte past it is 0xFF (p.8).
 */

#ifndef HW_MODEL_SGP30_H
#define HW_MODEL_SGP30_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SGP30 commands --- */
#define SGP30_CMD_IAQ_INIT       0x2003u
#define SGP30_CMD_MEASURE_IAQ    0x2008u
#define SGP30_CMD_GET_BASELINE   0x2015u
#define SGP30_CMD_SET_BASELINE   0x201Eu
#define SGP30_CMD_GET_SERIAL     0x3682u
#define SGP30_CMD_SET_HUMIDITY   0x2061u

/* --- Simulated environment --- */
typedef struct {
    /* Measurement state */
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;

    /* Baseline state */
    uint16_t tvoc_baseline;
    uint16_t eco2_baseline;

    /* Serial ID (3 words = 6 bytes) */
    uint16_t serial_w0;
    uint16_t serial_w1;
    uint16_t serial_w2;

    /* Humidity compensation (8.8 fixed-point g/m3) */
    uint16_t abs_humidity;

    /* Command tracking */
    uint16_t last_cmd;          /* command whose response is waiting, if any */
    bool     iaq_initialized;   /* iaq_init has been called */
    uint32_t busy_us;           /* time left of the running command */

    /* Counters for test introspection */
    int      init_count;
    int      measure_count;
    int      write_count;
    int      read_count;
    int      aborted;           /* transfers refused while busy */
} sgp30_model_t;

/* --- CRC-8 (Sensirion: poly 0x31, init 0xFF) --- */
static uint8_t sgp30_model_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80u)
                crc = (uint8_t)((crc << 1) ^ 0x31u);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Pack a 16-bit word into a 3-byte CRC-protected triplet */
static void sgp30_model_pack_word(uint8_t *out, uint16_t val)
{
    out[0] = (uint8_t)(val >> 8);
    out[1] = (uint8_t)(val & 0xFFu);
    out[2] = sgp30_model_crc8(out, 2);
}

/* Table 10, p.11, maximum duration. p.12: tIDLE for the serial ID. */
static uint32_t sgp30_model_duration_us(uint16_t cmd)
{
    switch (cmd) {
    case SGP30_CMD_IAQ_INIT:
    case SGP30_CMD_GET_BASELINE:
    case SGP30_CMD_SET_BASELINE:
    case SGP30_CMD_SET_HUMIDITY:
        return 10000u;
    case SGP30_CMD_MEASURE_IAQ:
        return 12000u;
    case SGP30_CMD_GET_SERIAL:
        return 500u;
    default:
        return 0u;
    }
}

/* --- I2C HAL callbacks --- */

/*
 * Raw write callback: parse 2-byte command, optionally followed by
 * data words (each 2 bytes + 1 CRC byte).
 */
__attribute__((unused))
static bool sgp30_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    sgp30_model_t *m = (sgp30_model_t *)ctx;
    m->write_count++;

    /* p.8: "no communication with the sensor is possible" */
    if (m->busy_us != 0) {
        m->aborted++;
        return false;
    }

    if (len < 2) return true;

    uint16_t cmd = (uint16_t)((uint16_t)data[0] << 8 | data[1]);
    bool accepted = true;
    m->last_cmd = cmd;

    switch (cmd) {
    case SGP30_CMD_IAQ_INIT:
        m->iaq_initialized = true;
        m->init_count++;
        break;

    case SGP30_CMD_MEASURE_IAQ:
        m->measure_count++;
        break;

    case SGP30_CMD_GET_BASELINE:
    case SGP30_CMD_GET_SERIAL:
        /* Read-only commands, no data payload */
        break;

    case SGP30_CMD_SET_BASELINE:
        /* Expects 2 data words: TVOC_bl (word 0) + eCO2_bl (word 1). p.8: the
         * sensor "only accepts data if it is followed by the correct checksum" */
        accepted = len == 8 &&
                   sgp30_model_crc8(&data[2], 2) == data[4] &&
                   sgp30_model_crc8(&data[5], 2) == data[7];
        if (accepted) {
            m->tvoc_baseline = (uint16_t)((uint16_t)data[2] << 8 | data[3]);
            m->eco2_baseline = (uint16_t)((uint16_t)data[5] << 8 | data[6]);
        }
        break;

    case SGP30_CMD_SET_HUMIDITY:
        /* Expects 1 data word: abs_humidity */
        accepted = len == 5 && sgp30_model_crc8(&data[2], 2) == data[4];
        if (accepted) {
            m->abs_humidity = (uint16_t)((uint16_t)data[2] << 8 | data[3]);
        }
        break;

    default:
        break;
    }

    if (accepted) m->busy_us = sgp30_model_duration_us(cmd);
    return true;
}

/*
 * Raw read callback: return CRC-protected response for the last command.
 */
__attribute__((unused))
static bool sgp30_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    sgp30_model_t *m = (sgp30_model_t *)ctx;
    m->read_count++;

    /* p.8: "the sensor aborts the communication with a XCK condition" */
    if (m->busy_us != 0) {
        m->aborted++;
        return false;
    }

    uint8_t response[9];
    uint16_t response_len = 0;

    switch (m->last_cmd) {
    case SGP30_CMD_MEASURE_IAQ:
        /* 2 words: eCO2 (word 0) + TVOC (word 1) */
        sgp30_model_pack_word(&response[0], m->eco2_ppm);
        sgp30_model_pack_word(&response[3], m->tvoc_ppb);
        response_len = 6;
        break;

    case SGP30_CMD_GET_BASELINE:
        /* 2 words: eCO2_bl (word 0) + TVOC_bl (word 1) */
        sgp30_model_pack_word(&response[0], m->eco2_baseline);
        sgp30_model_pack_word(&response[3], m->tvoc_baseline);
        response_len = 6;
        break;

    case SGP30_CMD_GET_SERIAL:
        /* 3 words: serial_w0, serial_w1, serial_w2 */
        sgp30_model_pack_word(&response[0], m->serial_w0);
        sgp30_model_pack_word(&response[3], m->serial_w1);
        sgp30_model_pack_word(&response[6], m->serial_w2);
        response_len = 9;
        break;

    default:
        /* Unknown command or no response expected */
        break;
    }

    /* p.8: "the data cannot be read more than once, and access to data beyond
     * the specified amount will return a pattern of 1s" */
    memset(data, 0xFF, len);
    memcpy(data, response, (len < response_len) ? len : response_len);
    m->last_cmd = 0;

    return true;
}

/* Delay callback: the running command finishes while the master waits */
__attribute__((unused))
static void sgp30_model_delay(void *ctx, uint16_t ms)
{
    sgp30_model_t *m = (sgp30_model_t *)ctx;
    uint32_t us = (uint32_t)ms * 1000u;

    m->busy_us = (us >= m->busy_us) ? 0u : m->busy_us - us;
}

static void sgp30_model_init(sgp30_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Defaults: clean air after warm-up */
    m->tvoc_ppb       = 0;
    m->eco2_ppm       = 400;
    m->tvoc_baseline  = 0x1234;
    m->eco2_baseline  = 0x5678;
    m->serial_w0      = 0x0000;
    m->serial_w1      = 0x0140;
    m->serial_w2      = 0x7FCA;
    m->abs_humidity   = 0x0000;  /* compensation disabled */
}

/* --- Helpers for tests --- */

__attribute__((unused))
static void sgp30_model_set_iaq(sgp30_model_t *m,
                                uint16_t eco2_ppm, uint16_t tvoc_ppb)
{
    m->eco2_ppm = eco2_ppm;
    m->tvoc_ppb = tvoc_ppb;
}

__attribute__((unused))
static void sgp30_model_set_baseline(sgp30_model_t *m,
                                     uint16_t eco2_bl, uint16_t tvoc_bl)
{
    m->eco2_baseline = eco2_bl;
    m->tvoc_baseline = tvoc_bl;
}

__attribute__((unused))
static void sgp30_model_set_serial(sgp30_model_t *m,
                                   uint16_t w0, uint16_t w1, uint16_t w2)
{
    m->serial_w0 = w0;
    m->serial_w1 = w1;
    m->serial_w2 = w2;
}

#endif /* HW_MODEL_SGP30_H */
