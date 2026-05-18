// SPDX-License-Identifier: MIT
/*
 * hw_model_bh1750.h -- BH1750FVI ambient light sensor hardware model
 *
 * Models ROHM BH1750FVI behavior:
 * - Command-based I2C (no register addresses)
 * - Power on (0x01), power off (0x00), reset (0x07)
 * - Measurement modes: 0x10/0x11/0x13 (continuous), 0x20/0x21/0x23 (one-shot)
 * - Read returns 2-byte big-endian raw light value
 * - Different resolution modes (H-res, H-res2, L-res)
 */

#ifndef HW_MODEL_BH1750_H
#define HW_MODEL_BH1750_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    bool     powered;
    uint8_t  mode;          /* active measurement mode command */
    uint16_t raw_light;     /* simulated raw light value (big-endian in read) */
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

/* --- I2C HAL callbacks (command-based) --- */

__attribute__((unused))
static bool bh1750_model_i2c_write(void *ctx, uint8_t dev_addr,
                                   const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    bh1750_model_t *m = (bh1750_model_t *)ctx;
    if (len < 1) return true;

    uint8_t cmd = data[0];
    m->last_cmd = cmd;
    m->cmd_count++;

    switch (cmd) {
    case 0x01:  /* Power on */
        m->powered = true;
        break;
    case 0x00:  /* Power off */
        m->powered = false;
        break;
    case 0x07:  /* Reset */
        m->raw_light = 0;
        break;
    case 0x10: case 0x11: case 0x13:  /* Continuous modes */
    case 0x20: case 0x21: case 0x23:  /* One-shot modes */
        m->mode = cmd;
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
    (void)dev_addr;
    bh1750_model_t *m = (bh1750_model_t *)ctx;
    m->read_count++;

    if (len >= 2) {
        data[0] = (uint8_t)(m->raw_light >> 8);
        data[1] = (uint8_t)(m->raw_light & 0xFF);
    }
    return true;
}

/* Helper: set simulated raw light value */
static void bh1750_model_set_raw(bh1750_model_t *m, uint16_t raw)
{
    m->raw_light = raw;
}

#endif /* HW_MODEL_BH1750_H */
