// SPDX-License-Identifier: MIT
/*
 * hw_model_opt3001.h — OPT3001 hardware register model from datasheet
 *
 * Models OPT3001 register behavior:
 * - 16-bit big-endian registers accessed via write_regs/read_regs
 * - Manufacturer ID at 0x7E: 0x5449 (read-only)
 * - Device ID at 0x7F: 0x3001 (read-only)
 * - Config register at 0x01: mode bits [10:9], CRF bit [7]
 * - Result register at 0x00: exponent[15:12] + mantissa[11:0] (read-only)
 *
 * NOTE: OPT3001 uses write_regs (multi-byte) instead of write_reg.
 *       We provide custom I2C callbacks that operate on 16-bit registers
 *       stored as two consecutive bytes in the hw_model (big-endian).
 */

#ifndef HW_MODEL_OPT3001_H
#define HW_MODEL_OPT3001_H

#include "hw_model.h"

/* --- OPT3001 register addresses --- */
#define OPT_REG_RESULT          0x00u
#define OPT_REG_CONFIG          0x01u
#define OPT_REG_MANUFACTURER_ID 0x7Eu
#define OPT_REG_DEVICE_ID       0x7Fu

/*
 * For 16-bit register devices, we store each 16-bit register as two
 * consecutive bytes in the model. We map register address R to model
 * bytes at offset R*2 and R*2+1 (big-endian: high byte first).
 */
#define OPT_BYTE_ADDR(reg) ((uint8_t)((reg) * 2u))

typedef struct {
    hw_reg_model_t  hw;
    float           true_lux;   /* simulated light level */
} opt3001_model_t;

/* Encode lux as OPT3001 result register: E[15:12] | M[11:0] */
static uint16_t opt3001_encode_lux(float lux)
{
    if (lux <= 0.0f) return 0;
    /* lux = 0.01 * 2^E * M; find smallest E such that M fits in 12 bits */
    uint8_t e;
    uint16_t mant;
    for (e = 0; e < 11; e++) {
        float scale = 0.01f * (float)(1u << e);
        mant = (uint16_t)(lux / scale);
        if (mant <= 0x0FFFu) break;
    }
    if (e >= 11) { e = 11; mant = 0x0FFF; }
    return (uint16_t)(((uint16_t)e << 12) | (mant & 0x0FFFu));
}

/* Write a 16-bit value into the model at the mapped byte address */
static void opt3001_model_set_reg16(opt3001_model_t *m, uint8_t reg,
                                     uint16_t val)
{
    uint8_t base = OPT_BYTE_ADDR(reg);
    m->hw.regs[base]     = (uint8_t)(val >> 8);
    m->hw.regs[base + 1] = (uint8_t)(val & 0xFF);
    m->hw.reset_values[base]     = (uint8_t)(val >> 8);
    m->hw.reset_values[base + 1] = (uint8_t)(val & 0xFF);
}

/* Callback: config register write — single-shot mode sets CRF and result */
static void opt3001_model_on_config_hi(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    opt3001_model_t *m = (opt3001_model_t *)user;
    /* Mode bits are in high byte bits [2:1] (bits 10:9 of 16-bit reg) */
    uint8_t mode = (new_val >> 1) & 0x03u;
    if (mode == 0x01u || mode == 0x03u) { /* single or continuous */
        /* Write result register */
        uint16_t encoded = opt3001_encode_lux(m->true_lux);
        uint8_t rbase = OPT_BYTE_ADDR(OPT_REG_RESULT);
        m->hw.regs[rbase]     = (uint8_t)(encoded >> 8);
        m->hw.regs[rbase + 1] = (uint8_t)(encoded & 0xFF);
        /* Set CRF bit in config low byte (bit 7 of low byte) */
        uint8_t cfg_lo_addr = OPT_BYTE_ADDR(OPT_REG_CONFIG) + 1;
        m->hw.regs[cfg_lo_addr] |= 0x80u;
    }
}

/* Custom I2C write_regs for 16-bit register model */
__attribute__((unused))
static bool opt3001_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                          uint8_t reg, const uint8_t *buf,
                                          uint16_t len)
{
    (void)dev_addr;
    opt3001_model_t *m = (opt3001_model_t *)ctx;
    uint8_t base = OPT_BYTE_ADDR(reg);
    for (uint16_t i = 0; i < len && (base + i) < HW_MODEL_REG_COUNT; i++) {
        hw_model__write_reg(&m->hw, (uint8_t)(base + i), buf[i]);
    }
    return true;
}

/* Custom I2C read_regs for 16-bit register model */
__attribute__((unused))
static bool opt3001_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                         uint8_t reg, uint8_t *buf,
                                         uint16_t len)
{
    (void)dev_addr;
    opt3001_model_t *m = (opt3001_model_t *)ctx;
    uint8_t base = OPT_BYTE_ADDR(reg);
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)base + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->hw.regs[idx] : 0xFF;
    }
    return true;
}

static void opt3001_model_init(opt3001_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Manufacturer ID: 0x5449 at register 0x7E (read-only) */
    uint8_t mfr_base = OPT_BYTE_ADDR(OPT_REG_MANUFACTURER_ID);
    m->hw.reset_values[mfr_base]     = 0x54u;
    m->hw.reset_values[mfr_base + 1] = 0x49u;
    hw_model_set_readonly(&m->hw, mfr_base);
    hw_model_set_readonly(&m->hw, (uint8_t)(mfr_base + 1));

    /* Device ID: 0x3001 at register 0x7F (read-only) */
    uint8_t dev_base = OPT_BYTE_ADDR(OPT_REG_DEVICE_ID);
    m->hw.reset_values[dev_base]     = 0x30u;
    m->hw.reset_values[dev_base + 1] = 0x01u;
    hw_model_set_readonly(&m->hw, dev_base);
    hw_model_set_readonly(&m->hw, (uint8_t)(dev_base + 1));

    /* Result register (read-only) */
    uint8_t res_base = OPT_BYTE_ADDR(OPT_REG_RESULT);
    hw_model_set_readonly(&m->hw, res_base);
    hw_model_set_readonly(&m->hw, (uint8_t)(res_base + 1));

    /* Config register write callback (high byte contains mode bits) */
    uint8_t cfg_base = OPT_BYTE_ADDR(OPT_REG_CONFIG);
    hw_model_set_on_write(&m->hw, cfg_base, opt3001_model_on_config_hi);

    /* CRF bit (bit 7 of config low byte) is read-only hardware flag.
     * Mask out bit 7 so driver writes cannot clear it — only the
     * measurement engine (our callback) sets/clears it. */
    hw_model_set_writable_mask(&m->hw, (uint8_t)(cfg_base + 1), 0x7Fu);

    /* Default config: 0xC010 (auto range, 100ms, shutdown) */
    m->hw.reset_values[cfg_base]     = 0xC0u;
    m->hw.reset_values[cfg_base + 1] = 0x10u;

    /* Default simulated lux: 500.0 */
    m->true_lux = 500.0f;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_OPT3001_H */
