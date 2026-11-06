// SPDX-License-Identifier: MIT
/*
 * hw_model_bh1750.h -- BH1750FVI ambient light sensor hardware model
 *
 * Models ROHM BH1750FVI behavior:
 * - Command-based I2C (no register addresses)
 * - Power on (0x01), power off (0x00), reset (0x07)
 * - Measurement modes: 0x10/0x11/0x13 (continuous), 0x20/0x21/0x23 (one-shot)
 * - Read returns the 2-byte big-endian Data Register, which starts at 0 and
 *   only changes when a measurement ends (p.4)
 * - A measurement takes the manual's maximum, 180 ms or 24 ms in L mode (p.2),
 *   so a driver that waits less sees the old value, as on a slow chip
 * - Different resolution modes (H-res, H-res2, L-res)
 * - Reset works only with the power on (p.5)
 * - The two addresses of the ADDR pin get an ACK, and no others (p.7)
 */

#ifndef HW_MODEL_BH1750_H
#define HW_MODEL_BH1750_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    bool     powered;
    uint8_t  mode;          /* active measurement mode command */
    uint16_t raw_light;     /* simulated light, as the count a measurement gives */
    uint16_t data_reg;      /* Data Register: what a read returns */
    uint32_t measure_left_ms;  /* time left in the measurement, 0 = none running */
    uint32_t delay_total_ms;   /* everything the driver has waited */
    int      measure_count;    /* measurements finished */
    int      cmd_count;     /* total commands received */
    int      read_count;
    uint8_t  last_cmd;
} bh1750_model_t;

static void bh1750_model_init(bh1750_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->powered = false;
    m->mode = 0;
    m->raw_light = 1200;  /* ~1000 lux in H-res mode (1200 / 1.2 = 1000) */
    m->cmd_count = 0;
    m->read_count = 0;
    m->last_cmd = 0xFF;
}

/* p.7: 0100011 with ADDR low, 1011100 with ADDR high */
static bool bh1750_model_addressed(uint8_t dev_addr)
{
    return dev_addr == 0x23 || dev_addr == 0x5C;
}

static uint32_t bh1750_model_measure_ms(uint8_t mode)
{
    return (mode & 0x03) == 0x03 ? 24u : 180u;   /* 0x13 and 0x23 are L mode */
}

/* Let simulated time pass. A measurement that ends copies the light into the
 * Data Register. A continuous mode then starts the next one, and a one-time
 * mode goes to Power Down (p.5), which keeps the data readable (p.7). */
__attribute__((unused))
static void bh1750_model_advance(bh1750_model_t *m, uint32_t ms)
{
    while (m->measure_left_ms != 0 && ms >= m->measure_left_ms) {
        ms -= m->measure_left_ms;
        m->data_reg = m->raw_light;
        m->measure_count++;
        if (m->mode & 0x20) {
            m->measure_left_ms = 0;
            m->powered = false;
        } else {
            m->measure_left_ms = bh1750_model_measure_ms(m->mode);
        }
    }
    if (m->measure_left_ms != 0) {
        m->measure_left_ms -= ms;
    }
}

/* --- I2C HAL callbacks (command-based) --- */

__attribute__((unused))
static bool bh1750_model_i2c_write(void *ctx, uint8_t dev_addr,
                                   const uint8_t *data, uint16_t len)
{
    bh1750_model_t *m = (bh1750_model_t *)ctx;
    if (!bh1750_model_addressed(dev_addr)) return false;
    if (len < 1) return true;

    uint8_t cmd = data[0];
    m->last_cmd = cmd;
    m->cmd_count++;

    switch (cmd) {
    case 0x01:  /* Power on: waits for a measurement command (p.5) */
        m->powered = true;
        break;
    case 0x00:  /* Power off */
        m->powered = false;
        m->measure_left_ms = 0;
        break;
    case 0x07:  /* Reset: "not acceptable in Power Down mode" (p.5) */
        if (m->powered)
            m->data_reg = 0;
        break;
    case 0x10: case 0x11: case 0x13:  /* Continuous modes */
    case 0x20: case 0x21: case 0x23:  /* One-shot modes */
        m->mode = cmd;
        m->powered = true;  /* Power On "is possible to omit" (p.4) */
        m->measure_left_ms = bh1750_model_measure_ms(cmd);
        break;
    default:
        break;
    }
    return true;
}

__attribute__((unused))
static bool bh1750_model_i2c_read(void *ctx, uint8_t dev_addr,
                                  uint8_t *data, uint16_t len)
{
    bh1750_model_t *m = (bh1750_model_t *)ctx;
    if (!bh1750_model_addressed(dev_addr)) return false;
    m->read_count++;

    if (len >= 2) {
        data[0] = (uint8_t)(m->data_reg >> 8);
        data[1] = (uint8_t)(m->data_reg & 0xFF);
    }
    return true;
}

__attribute__((unused))
static void bh1750_model_delay_ms(void *ctx, uint32_t ms)
{
    bh1750_model_t *m = (bh1750_model_t *)ctx;
    m->delay_total_ms += ms;
    bh1750_model_advance(m, ms);
}

/* Helper: set the simulated light, which the next measurement to end picks up */
static void bh1750_model_set_raw(bh1750_model_t *m, uint16_t raw)
{
    m->raw_light = raw;
}

#endif /* HW_MODEL_BH1750_H */
