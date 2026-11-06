// SPDX-License-Identifier: MIT
/*
 * hw_model_ms5611.h — MS5611 hardware model from datasheet
 *
 * Models MS5611-01BA03 command-based protocol:
 * - No chip ID register; PROM calibration at 0xA0..0xAE
 * - Reset (0x1E), D1/D2 conversion, ADC read (0x00)
 * - Each byte written is a command (p10), and none is discarded. The manual
 *   does not say what the part does with a second byte in a message, so the
 *   model takes the worst reading: a 0x00 after a Convert is an ADC Read during
 *   the conversion, which p11 says gives 0 and spoils the result
 * - read_regs sends its command byte, then reads the answer
 * - No clock: the driver's wait for a conversion is what ends it
 */

#ifndef HW_MODEL_MS5611_H
#define HW_MODEL_MS5611_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint16_t prom[8];
    uint32_t adc_result;
    uint32_t d1_raw;
    uint32_t d2_raw;
    uint8_t  last_cmd;
    int      reset_count;
    int      convert_count;
    bool     converting;
} ms5611_model_t;

static void ms5611_model_init(ms5611_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* Datasheet example calibration. Word 0 and word 7 are from ArduPilot
     * SIM_MS5611.h, which gives this PROM a valid CRC in DB3-DB0. */
    m->prom[0] = 0xFFFF;
    m->prom[1] = 40127;    /* C1 */
    m->prom[2] = 36924;    /* C2 */
    m->prom[3] = 23317;    /* C3 */
    m->prom[4] = 23282;    /* C4 */
    m->prom[5] = 33464;    /* C5 */
    m->prom[6] = 28312;    /* C6 */
    m->prom[7] = 0x0008;   /* CRC */

    /* Default ADC values from datasheet example */
    m->d1_raw = 9085466;
    m->d2_raw = 8569150;
}

static void ms5611_model_command(ms5611_model_t *m, uint8_t cmd)
{
    m->last_cmd = cmd;

    if (cmd == 0x1E) {
        m->reset_count++;
    } else if (cmd >= 0x40 && cmd <= 0x48) {
        m->adc_result = m->d1_raw;
        m->convert_count++;
        m->converting = true;
    } else if (cmd >= 0x50 && cmd <= 0x58) {
        m->adc_result = m->d2_raw;
        m->convert_count++;
        m->converting = true;
    } else if (cmd == 0x00 && m->converting) {
        m->adc_result = 0;
    }
}

__attribute__((unused))
static bool ms5611_model_write_raw(void *ctx, uint8_t dev_addr,
                                   const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    ms5611_model_t *m = (ms5611_model_t *)ctx;

    for (uint16_t i = 0; i < len; i++) {
        ms5611_model_command(m, data[i]);
    }
    return true;
}

/* A register-style write: the data byte reaches the chip like any other */
__attribute__((unused))
static bool ms5611_model_write(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = { reg, val };

    return ms5611_model_write_raw(ctx, dev_addr, tx, 2);
}

__attribute__((unused))
static bool ms5611_model_read(void *ctx, uint8_t dev_addr,
                              uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    ms5611_model_t *m = (ms5611_model_t *)ctx;

    ms5611_model_command(m, reg);

    if (m->last_cmd >= 0xA0 && m->last_cmd <= 0xAE && len >= 2) {
        uint8_t idx = (uint8_t)((m->last_cmd - 0xA0) / 2);
        buf[0] = (uint8_t)(m->prom[idx] >> 8);
        buf[1] = (uint8_t)(m->prom[idx] & 0xFF);
        return true;
    }

    if (m->last_cmd == 0x00 && len >= 3) {
        buf[0] = (uint8_t)(m->adc_result >> 16);
        buf[1] = (uint8_t)(m->adc_result >> 8);
        buf[2] = (uint8_t)(m->adc_result & 0xFF);
        return true;
    }

    memset(buf, 0, len);
    return true;
}

__attribute__((unused))
static void ms5611_model_delay(void *ctx, uint32_t ms)
{
    (void)ms;
    ((ms5611_model_t *)ctx)->converting = false;
}

#endif /* HW_MODEL_MS5611_H */
