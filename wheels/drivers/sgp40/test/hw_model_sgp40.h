// SPDX-License-Identifier: MIT
/*
 * hw_model_sgp40.h — SGP40 hardware model from datasheet
 *
 * Models Sensirion SGP40 command-based protocol:
 * - Self-test (0x280E) returns 0xD400 + CRC
 * - Measure raw (0x260F) returns SRAW + CRC
 * - CRC-8: poly 0x31, init 0xFF
 */

#ifndef HW_MODEL_SGP40_H
#define HW_MODEL_SGP40_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Forward declaration — uses sgp40_crc8 from the driver */
extern uint8_t sgp40_crc8(const uint8_t *data, uint16_t len);

typedef struct {
    uint16_t sraw_value;
    bool     self_test_pass;
    uint8_t  last_cmd_msb;
    uint8_t  last_cmd_lsb;
    int      measure_count;
    int      self_test_count;
} sgp40_model_t;

static void sgp40_model_init(sgp40_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->sraw_value     = 26000;
    m->self_test_pass = true;
}

__attribute__((unused))
static bool sgp40_model_write(void *ctx, uint8_t dev_addr,
                              uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    sgp40_model_t *m = (sgp40_model_t *)ctx;
    m->last_cmd_msb = reg;
    m->last_cmd_lsb = val;

    if (reg == 0x28 && val == 0x0E) {
        m->self_test_count++;
    } else if (reg == 0x26 && val == 0x0F) {
        m->measure_count++;
    }
    return true;
}

__attribute__((unused))
static bool sgp40_model_read(void *ctx, uint8_t dev_addr,
                             uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr; (void)reg;
    sgp40_model_t *m = (sgp40_model_t *)ctx;

    uint16_t word;
    if (m->last_cmd_msb == 0x28 && m->last_cmd_lsb == 0x0E) {
        /* Self-test response */
        word = m->self_test_pass ? 0xD400 : 0x4B00;
    } else {
        /* Measurement response */
        word = m->sraw_value;
    }

    if (len >= 3) {
        buf[0] = (uint8_t)(word >> 8);
        buf[1] = (uint8_t)(word & 0xFF);
        buf[2] = sgp40_crc8(buf, 2);
    }
    return true;
}

__attribute__((unused))
static void sgp40_model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_SGP40_H */
