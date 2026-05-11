/*
 * hw_model_ms5611.h — MS5611 hardware model from datasheet
 *
 * Models MS5611-01BA03 command-based protocol:
 * - No chip ID register; PROM calibration at 0xA0..0xAE
 * - Reset (0x1E), D1/D2 conversion, ADC read (0x00)
 * - Uses the I2C write_reg/read_regs pattern as command/response
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
} ms5611_model_t;

static void ms5611_model_init(ms5611_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* Datasheet example calibration */
    m->prom[0] = 0;
    m->prom[1] = 40127;    /* C1 */
    m->prom[2] = 36924;    /* C2 */
    m->prom[3] = 23317;    /* C3 */
    m->prom[4] = 23282;    /* C4 */
    m->prom[5] = 33464;    /* C5 */
    m->prom[6] = 28312;    /* C6 */
    m->prom[7] = 0;

    /* Default ADC values from datasheet example */
    m->d1_raw = 9085466;
    m->d2_raw = 8569150;
}

__attribute__((unused))
static bool ms5611_model_write(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t val)
{
    (void)dev_addr; (void)val;
    ms5611_model_t *m = (ms5611_model_t *)ctx;
    m->last_cmd = reg;

    if (reg == 0x1E) {
        m->reset_count++;
    } else if (reg >= 0x40 && reg <= 0x48) {
        m->adc_result = m->d1_raw;
        m->convert_count++;
    } else if (reg >= 0x50 && reg <= 0x58) {
        m->adc_result = m->d2_raw;
        m->convert_count++;
    }
    return true;
}

__attribute__((unused))
static bool ms5611_model_read(void *ctx, uint8_t dev_addr,
                              uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr; (void)reg;
    ms5611_model_t *m = (ms5611_model_t *)ctx;

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
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_MS5611_H */
