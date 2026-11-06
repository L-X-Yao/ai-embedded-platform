// SPDX-License-Identifier: MIT
/*
 * hw_model_sgp40.h — SGP40 hardware model from datasheet
 *
 * Models the Sensirion SGP40 command protocol (datasheet v1.2), with the
 * raw write and read of sgp40_i2c_t:
 * - Self-test (0x280E) answers 0xD4 0xXX or 0x4B 0xXX + CRC after 320 ms.
 *   The second byte is undefined, so tests choose it (Table 13, p14)
 * - Measure raw (0x260F + 2 words with CRC) answers SRAW + CRC after 30 ms
 * - Heater off (0x3615) ends measurement mode. A self-test keeps the heater
 *   as it was (p10)
 * - While a command runs the sensor NACKs everything, and only 0x59 answers
 *   (p11). Times are the Table 8 maxima, and time passes through delay_ms
 * - CRC-8: poly 0x31, init 0xFF
 *
 * The manual does not say what the sensor does with a bad CRC on a word it
 * receives, so the model counts it and goes on. A read with nothing to
 * collect is NACKed. The serial number command is not modelled.
 */

#ifndef HW_MODEL_SGP40_H
#define HW_MODEL_SGP40_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../sgp40.h"

#define SGP40_T_SELF_TEST_MS  320u
#define SGP40_T_MEASURE_MS    30u
#define SGP40_T_HEATER_OFF_MS 1u

typedef enum {
    SGP40_MODEL_NOTHING,
    SGP40_MODEL_SELF_TEST,
    SGP40_MODEL_MEASURE,
} sgp40_model_pending_t;

typedef struct {
    uint16_t sraw_value;
    bool     self_test_pass;
    uint8_t  self_test_low;     /* the 0xXX byte of the self-test answer */
    uint8_t  last_params[6];    /* words and CRC bytes of the last measure */
    sgp40_model_pending_t pending;
    uint32_t busy_ms;           /* until the running command ends */
    bool     heater_on;         /* measurement mode */
    int      measure_count;
    int      self_test_count;
    int      bad_crc_count;
    int      bad_cmd_count;
} sgp40_model_t;

static void sgp40_model_init(sgp40_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->sraw_value     = 26000;
    m->self_test_pass = true;
    m->self_test_low  = 0x5A;
}

__attribute__((unused))
static bool sgp40_model_write(void *ctx, uint8_t dev_addr,
                              const uint8_t *data, uint16_t len)
{
    sgp40_model_t *m = (sgp40_model_t *)ctx;

    if (dev_addr != SGP40_DEV_ADDR || m->busy_ms > 0) return false;

    if (len == 2 && data[0] == 0x28 && data[1] == 0x0E) {
        m->self_test_count++;
        m->pending = SGP40_MODEL_SELF_TEST;
        m->busy_ms = SGP40_T_SELF_TEST_MS;
        return true;
    }
    if (len == 8 && data[0] == 0x26 && data[1] == 0x0F) {
        if (sgp40_crc8(&data[2], 2) != data[4]) m->bad_crc_count++;
        if (sgp40_crc8(&data[5], 2) != data[7]) m->bad_crc_count++;
        memcpy(m->last_params, &data[2], sizeof(m->last_params));
        m->measure_count++;
        m->heater_on = true;
        m->pending = SGP40_MODEL_MEASURE;
        m->busy_ms = SGP40_T_MEASURE_MS;
        return true;
    }
    if (len == 2 && data[0] == 0x36 && data[1] == 0x15) {
        m->heater_on = false;
        m->pending = SGP40_MODEL_NOTHING;
        m->busy_ms = SGP40_T_HEATER_OFF_MS;
        return true;
    }
    m->bad_cmd_count++;
    return false;
}

__attribute__((unused))
static bool sgp40_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t *data, uint16_t len)
{
    sgp40_model_t *m = (sgp40_model_t *)ctx;
    uint8_t answer[3];

    if (dev_addr != SGP40_DEV_ADDR || m->busy_ms > 0) return false;

    if (m->pending == SGP40_MODEL_SELF_TEST) {
        answer[0] = m->self_test_pass ? 0xD4 : 0x4B;
        answer[1] = m->self_test_low;
    } else if (m->pending == SGP40_MODEL_MEASURE) {
        answer[0] = (uint8_t)(m->sraw_value >> 8);
        answer[1] = (uint8_t)(m->sraw_value & 0xFF);
    } else {
        return false;
    }
    answer[2] = sgp40_crc8(answer, 2);
    m->pending = SGP40_MODEL_NOTHING;

    /* The master may stop early, after any byte (p12) */
    memcpy(data, answer, len < sizeof(answer) ? len : sizeof(answer));
    return true;
}

__attribute__((unused))
static void sgp40_model_delay(void *ctx, uint32_t ms)
{
    sgp40_model_t *m = (sgp40_model_t *)ctx;
    m->busy_ms = ms >= m->busy_ms ? 0 : m->busy_ms - ms;
}

#endif /* HW_MODEL_SGP40_H */
