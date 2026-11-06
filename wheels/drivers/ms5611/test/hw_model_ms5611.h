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
 * - Time moves with the delay callback. A conversion lasts the p3 maximum for
 *   its OSR, and the PROM loads 2.8 ms after a Reset (p10). Before that the
 *   model reads it as 0: the manual only says Reset makes sure it gets loaded
 * - p11: an ADC Read gives 0 with no finished conversion or when repeated. An
 *   ADC Read or a Convert during a conversion spoils it, and the model takes
 *   0 as the wrong result the manual promises
 */

#ifndef HW_MODEL_MS5611_H
#define HW_MODEL_MS5611_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* p10 Reset figure, "2.8ms RELOAD" */
#define MS5611_T_RELOAD_US 2800u

typedef struct {
    uint16_t prom[8];
    uint32_t adc_result;      /* what the next ADC Read gets */
    uint32_t adc_out;         /* what the last ADC Read got */
    uint32_t pending;         /* result of the running conversion */
    uint32_t d1_raw;
    uint32_t d2_raw;
    uint8_t  last_cmd;
    int      reset_count;
    int      convert_count;
    bool     converting;
    bool     spoiled;         /* the running conversion will end up wrong */
    bool     reloading;
    bool     prom_loaded;
    uint32_t now_us;          /* model clock */
    uint32_t convert_done_us;
    uint32_t reload_done_us;
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

/* Moves the clock and ends what has run its time. */
static void ms5611_model__advance(ms5611_model_t *m, uint32_t us)
{
    m->now_us += us;
    if (m->converting && m->now_us >= m->convert_done_us) {
        m->converting = false;
        m->adc_result = m->spoiled ? 0u : m->pending;
    }
    if (m->reloading && m->now_us >= m->reload_done_us) {
        m->reloading   = false;
        m->prom_loaded = true;
    }
}

/* p10: 0x40-0x48 Convert D1, 0x50-0x58 Convert D2, even codes by OSR */
static void ms5611_model__convert(ms5611_model_t *m, uint8_t cmd,
                                  uint32_t result)
{
    /* p3 conversion time, maximum, for OSR 256, 512, 1024, 2048 and 4096 */
    static const uint32_t tc_max_us[5] = { 600, 1170, 2280, 4540, 9040 };

    m->convert_count++;

    /* p11 "Conversion sequence sent during the already started conversion
     * process will yield incorrect result as well." */
    if (m->converting) {
        m->spoiled = true;
        return;
    }

    m->converting      = true;
    m->spoiled         = false;
    m->pending         = result;
    m->adc_result      = 0;
    m->convert_done_us = m->now_us + tc_max_us[(cmd & 0x0Fu) / 2u];
}

static void ms5611_model_command(ms5611_model_t *m, uint8_t cmd)
{
    m->last_cmd = cmd;

    if (cmd == 0x1E) {
        m->reset_count++;
        m->converting     = false;
        m->adc_result     = 0;
        m->prom_loaded    = false;
        m->reloading      = true;
        m->reload_done_us = m->now_us + MS5611_T_RELOAD_US;
    } else if (cmd >= 0x40 && cmd <= 0x48 && !(cmd & 1u)) {
        ms5611_model__convert(m, cmd, m->d1_raw);
    } else if (cmd >= 0x50 && cmd <= 0x58 && !(cmd & 1u)) {
        ms5611_model__convert(m, cmd, m->d2_raw);
    } else if (cmd == 0x00) {
        if (m->converting) {
            /* p11 "the result will be 0, the conversion will not stop and
             * the final result will be wrong" */
            m->spoiled = true;
            m->adc_out = 0;
        } else {
            /* p11: the result goes out once, a repeat gives 0 */
            m->adc_out    = m->adc_result;
            m->adc_result = 0;
        }
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
        uint16_t word = m->prom_loaded ? m->prom[idx] : 0u;
        buf[0] = (uint8_t)(word >> 8);
        buf[1] = (uint8_t)(word & 0xFF);
        return true;
    }

    if (m->last_cmd == 0x00 && len >= 3) {
        buf[0] = (uint8_t)(m->adc_out >> 16);
        buf[1] = (uint8_t)(m->adc_out >> 8);
        buf[2] = (uint8_t)(m->adc_out & 0xFF);
        return true;
    }

    memset(buf, 0, len);
    return true;
}

__attribute__((unused))
static void ms5611_model_delay(void *ctx, uint32_t ms)
{
    ms5611_model__advance((ms5611_model_t *)ctx, ms * 1000u);
}

#endif /* HW_MODEL_MS5611_H */
