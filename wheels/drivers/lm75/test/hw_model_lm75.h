// SPDX-License-Identifier: MIT
/*
 * hw_model_lm75.h -- LM75 hardware register model from datasheet
 *
 * Models LM75/LM75A/LM75B register behavior:
 * - Reg 0x00: Temperature (read-only, 2 bytes, 9-bit two's complement)
 * - Reg 0x01: Configuration (R/W, 1 byte)
 * - Reg 0x02: T_hyst (R/W, 2 bytes, same encoding as temperature)
 * - Reg 0x03: T_os (R/W, 2 bytes, same encoding as temperature)
 *
 * POR defaults per datasheet:
 *   Temperature = 0x0000 (0 C)
 *   Config = 0x00
 *   T_hyst = 0x4B00 (75 C)
 *   T_os = 0x5000 (80 C)
 */
#ifndef HW_MODEL_LM75_H
#define HW_MODEL_LM75_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* LM75 has only 4 registers but we model via a 2-byte-per-reg array.
 * Registers 0x00, 0x02, 0x03 are 2-byte; 0x01 is 1-byte. */

typedef struct {
    /* Register storage: 2 bytes per register pointer, 4 registers */
    uint8_t temp[2];      /* reg 0x00: read-only */
    uint8_t conf;         /* reg 0x01: R/W */
    uint8_t thyst[2];     /* reg 0x02: R/W */
    uint8_t tos[2];       /* reg 0x03: R/W */
    int     write_count;
    int     read_count;
} lm75_model_t;

static lm75_model_t lm75_mdl;

static void hw_model_reset(void)
{
    memset(&lm75_mdl, 0, sizeof(lm75_mdl));

    /* POR defaults from LM75 datasheet */
    lm75_mdl.temp[0] = 0x00;  /* 0 C */
    lm75_mdl.temp[1] = 0x00;
    lm75_mdl.conf    = 0x00;
    /* T_hyst POR = 75 C = 0x4B00 */
    lm75_mdl.thyst[0] = 0x4B;
    lm75_mdl.thyst[1] = 0x00;
    /* T_os POR = 80 C = 0x5000 */
    lm75_mdl.tos[0] = 0x50;
    lm75_mdl.tos[1] = 0x00;
}

/* Inject a temperature value (9-bit two's complement in bits [15:7]) */
static void hw_model_inject_temp(float temp_c)
{
    int16_t raw = (int16_t)(temp_c * 256.0f);
    /* Round to 9-bit boundary */
    raw = (int16_t)(raw & ~0x7F);
    lm75_mdl.temp[0] = (uint8_t)((uint16_t)raw >> 8);
    /* D0-D6 are undefined, so the model leaves them non-zero */
    lm75_mdl.temp[1] = (uint8_t)(((uint16_t)raw & 0x80u) | 0x55u);
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    lm75_mdl.write_count++;

    switch (reg) {
    case 0x00: /* Temperature is read-only -- silently ignore */
        break;
    case 0x01:
        lm75_mdl.conf = val;
        break;
    default:
        break;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_write_regs(void *ctx, uint8_t dev_addr,
                                 uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    lm75_mdl.write_count++;

    switch (reg) {
    case 0x00: /* read-only */
        break;
    case 0x01:
        if (len >= 1) lm75_mdl.conf = buf[0];
        break;
    case 0x02:
        if (len >= 1) lm75_mdl.thyst[0] = buf[0];
        if (len >= 2) lm75_mdl.thyst[1] = buf[1];
        break;
    case 0x03:
        if (len >= 1) lm75_mdl.tos[0] = buf[0];
        if (len >= 2) lm75_mdl.tos[1] = buf[1];
        break;
    default:
        break;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    lm75_mdl.read_count++;

    switch (reg) {
    case 0x00:
        if (len >= 1) buf[0] = lm75_mdl.temp[0];
        if (len >= 2) buf[1] = lm75_mdl.temp[1];
        break;
    case 0x01:
        if (len >= 1) buf[0] = lm75_mdl.conf;
        break;
    case 0x02:
        if (len >= 1) buf[0] = lm75_mdl.thyst[0];
        if (len >= 2) buf[1] = lm75_mdl.thyst[1];
        break;
    case 0x03:
        if (len >= 1) buf[0] = lm75_mdl.tos[0];
        if (len >= 2) buf[1] = lm75_mdl.tos[1];
        break;
    default:
        for (uint16_t i = 0; i < len; i++) buf[i] = 0xFF;
        break;
    }
    return true;
}

#endif /* HW_MODEL_LM75_H */
