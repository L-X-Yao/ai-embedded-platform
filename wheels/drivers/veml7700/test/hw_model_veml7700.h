// SPDX-License-Identifier: MIT
/*
 * hw_model_veml7700.h — VEML7700 hardware register model from datasheet
 *
 * Models VEML7700 register behavior:
 * - 16-bit little-endian registers
 * - ALS_CONF at register 0x00: gain[12:11], IT[9:6], SD[0]
 * - ALS data at register 0x04 (read-only)
 * - White data at register 0x05 (read-only)
 *
 * NOTE: VEML7700 uses write_regs (multi-byte) instead of write_reg.
 *       We provide custom I2C callbacks that operate on 16-bit registers
 *       stored as two consecutive bytes in the hw_model (little-endian).
 */

#ifndef HW_MODEL_VEML7700_H
#define HW_MODEL_VEML7700_H

#include "hw_model.h"

/* --- VEML7700 register addresses --- */
#define VEML_REG_ALS_CONF   0x00u
#define VEML_REG_ALS_DATA   0x04u
#define VEML_REG_WHITE_DATA 0x05u

/*
 * Map register R to model bytes at R*2 and R*2+1 (little-endian).
 */
#define VEML_BYTE_ADDR(reg) ((uint8_t)((reg) * 2u))

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_als;    /* simulated ALS raw count */
    uint16_t        true_white;  /* simulated White raw count */
} veml7700_model_t;

/* Custom I2C write_regs for 16-bit register model */
__attribute__((unused))
static bool veml7700_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                           uint8_t reg, const uint8_t *buf,
                                           uint16_t len)
{
    (void)dev_addr;
    veml7700_model_t *m = (veml7700_model_t *)ctx;
    uint8_t base = VEML_BYTE_ADDR(reg);
    for (uint16_t i = 0; i < len && (base + i) < HW_MODEL_REG_COUNT; i++) {
        hw_model__write_reg(&m->hw, (uint8_t)(base + i), buf[i]);
    }
    return true;
}

/* Custom I2C read_regs for 16-bit register model */
__attribute__((unused))
static bool veml7700_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                          uint8_t reg, uint8_t *buf,
                                          uint16_t len)
{
    (void)dev_addr;
    veml7700_model_t *m = (veml7700_model_t *)ctx;
    uint8_t base = VEML_BYTE_ADDR(reg);
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)base + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->hw.regs[idx] : 0xFF;
    }
    return true;
}

/* Set ALS and White data in model (little-endian) */
static void veml7700_model_set_data(veml7700_model_t *m)
{
    uint8_t als_base = VEML_BYTE_ADDR(VEML_REG_ALS_DATA);
    m->hw.regs[als_base]     = (uint8_t)(m->true_als & 0xFF);
    m->hw.regs[als_base + 1] = (uint8_t)(m->true_als >> 8);

    uint8_t w_base = VEML_BYTE_ADDR(VEML_REG_WHITE_DATA);
    m->hw.regs[w_base]     = (uint8_t)(m->true_white & 0xFF);
    m->hw.regs[w_base + 1] = (uint8_t)(m->true_white >> 8);
}

/* Callback: ALS_CONF low byte write — when SD=0, sensor is powered on */
static void veml7700_model_on_conf_lo(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    veml7700_model_t *m = (veml7700_model_t *)user;
    if (!(new_val & 0x01u)) { /* SD=0, powered on */
        veml7700_model_set_data(m);
    }
}

static void veml7700_model_init(veml7700_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* ALS data register (read-only) */
    uint8_t als_base = VEML_BYTE_ADDR(VEML_REG_ALS_DATA);
    hw_model_set_readonly(&m->hw, als_base);
    hw_model_set_readonly(&m->hw, (uint8_t)(als_base + 1));

    /* White data register (read-only) */
    uint8_t w_base = VEML_BYTE_ADDR(VEML_REG_WHITE_DATA);
    hw_model_set_readonly(&m->hw, w_base);
    hw_model_set_readonly(&m->hw, (uint8_t)(w_base + 1));

    /* Config register write callback (low byte has SD bit) */
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    hw_model_set_on_write(&m->hw, conf_base, veml7700_model_on_conf_lo);

    /* Default ALS_CONF: shutdown (SD=1) */
    m->hw.reset_values[conf_base] = 0x01u;

    /* Default simulated data */
    m->true_als   = 1000;
    m->true_white = 1200;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_VEML7700_H */
