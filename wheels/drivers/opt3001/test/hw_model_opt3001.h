// SPDX-License-Identifier: MIT
/*
 * hw_model_opt3001.h — OPT3001 hardware register model from datasheet
 *
 * Models OPT3001 register behavior:
 * - 16-bit big-endian registers accessed via write_regs/read_regs
 * - Manufacturer ID at 0x7E: 0x5449 (read-only)
 * - Device ID at 0x7F: 0x3001 (read-only)
 * - Config register at 0x01, reset C810h: mode bits [10:9], CRF bit [7]; OVF,
 *   CRF, FH and FL are read-only, and CRF clears on a config read and on a
 *   write that is not a shutdown
 * - Result register at 0x00: exponent[15:12] + mantissa[11:0] (read-only)
 * - A conversion takes time: call opt3001_model_advance_ms to let it finish.
 *   Single-shot then returns M to 00b, continuous (10b, 11b) starts the next
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

/* Config register byte fields (SBOS681C Table 10) */
#define OPT_CFG_HI_CT       0x08u   /* bit 11: 0 = 100 ms, 1 = 800 ms */
#define OPT_CFG_HI_M_MASK   0x06u   /* bits 10:9 */
#define OPT_CFG_HI_OVF      0x01u   /* bit 8, read-only */
#define OPT_CFG_LO_CRF      0x80u   /* bit 7, read-only */
#define OPT_CFG_HI_WRITABLE 0xFEu   /* RN, CT, M */
#define OPT_CFG_LO_WRITABLE 0x1Fu   /* L, POL, ME, FC; CRF, FH, FL are R */

/* Conversion length: total integration time maximum (p6) plus 3 ms (p5 note 5) */
#define OPT_CONV_MS_100     (110u + 3u)
#define OPT_CONV_MS_800     (880u + 3u)

/* Top of the device range: Table 9 last row, E = 0Bh, R = FFFh */
#define OPT_FULL_SCALE_LUX  83865.6f

typedef struct {
    hw_reg_model_t  hw;
    float           true_lux;     /* simulated light level */
    uint32_t        conv_left_ms; /* 0 = no conversion running */
} opt3001_model_t;

/* Encode lux as OPT3001 result register: E[15:12] | M[11:0] */
static uint16_t opt3001_encode_lux(float lux)
{
    if (lux <= 0.0f) return 0;
    /* lux = 0.01 * 2^E * R; find smallest E, up to 0Bh, such that R fits in
     * 12 bits. Compare before casting: the quotient can pass 16 bits. */
    for (uint16_t e = 0; e <= 11u; e++) {
        double r = (double)lux / (0.01 * (double)(1u << e));
        if (r < 4095.5)
            return (uint16_t)((e << 12) | (uint16_t)(r + 0.5));
    }
    return 0xBFFFu;
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

static uint32_t opt3001_model_conv_ms(const opt3001_model_t *m)
{
    uint8_t cfg_hi = m->hw.regs[OPT_BYTE_ADDR(OPT_REG_CONFIG)];
    return (cfg_hi & OPT_CFG_HI_CT) ? OPT_CONV_MS_800 : OPT_CONV_MS_100;
}

/* End of a conversion: new result, OVF reevaluated, CRF set, and the mode
 * field decides what follows. */
static void opt3001_model_finish_conversion(opt3001_model_t *m)
{
    uint16_t encoded = opt3001_encode_lux(m->true_lux);
    uint8_t rbase = OPT_BYTE_ADDR(OPT_REG_RESULT);
    uint8_t cfg = OPT_BYTE_ADDR(OPT_REG_CONFIG);

    m->hw.regs[rbase]     = (uint8_t)(encoded >> 8);
    m->hw.regs[rbase + 1] = (uint8_t)(encoded & 0xFF);
    if (m->true_lux > OPT_FULL_SCALE_LUX)
        m->hw.regs[cfg] |= OPT_CFG_HI_OVF;
    else
        m->hw.regs[cfg] &= (uint8_t)~OPT_CFG_HI_OVF;
    m->hw.regs[cfg + 1] |= OPT_CFG_LO_CRF;

    if (((m->hw.regs[cfg] & OPT_CFG_HI_M_MASK) >> 1) == 0x01u)
        m->hw.regs[cfg] &= (uint8_t)~OPT_CFG_HI_M_MASK;  /* single-shot done */
    else
        m->conv_left_ms = opt3001_model_conv_ms(m);      /* continuous */
}

/* Let simulated time pass, finishing any conversion that runs out */
__attribute__((unused))
static void opt3001_model_advance_ms(opt3001_model_t *m, uint32_t ms)
{
    while (ms > 0u && m->conv_left_ms > 0u) {
        uint32_t step = (ms < m->conv_left_ms) ? ms : m->conv_left_ms;
        ms -= step;
        m->conv_left_ms -= step;
        if (m->conv_left_ms == 0u)
            opt3001_model_finish_conversion(m);
    }
}

/* Callback on the config low byte: the word is complete once it lands. A
 * write aborts any running conversion and starts one unless M is 00b. Only
 * that non-shutdown write clears CRF. */
static void opt3001_model_on_config_lo(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    opt3001_model_t *m = (opt3001_model_t *)user;
    uint8_t cfg = OPT_BYTE_ADDR(OPT_REG_CONFIG);

    m->conv_left_ms = 0;
    if (m->hw.regs[cfg] & OPT_CFG_HI_M_MASK) {
        m->hw.regs[cfg + 1] &= (uint8_t)~OPT_CFG_LO_CRF;
        m->conv_left_ms = opt3001_model_conv_ms(m);
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
    if (len != 2u) return false;  /* p16: "The next two bytes" */
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
    if (len != 2u) return false;  /* p16: MSB then LSB of one register */
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)base + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->hw.regs[idx] : 0xFF;
    }
    /* p14 note 4: "Immediately after the configuration register is read, the
     * device automatically resets the conversion ready field" */
    if (reg == OPT_REG_CONFIG)
        m->hw.regs[base + 1] &= (uint8_t)~OPT_CFG_LO_CRF;
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

    /* Config register: acts when the low byte, the last of the word, lands */
    uint8_t cfg_base = OPT_BYTE_ADDR(OPT_REG_CONFIG);
    hw_model_set_on_write(&m->hw, (uint8_t)(cfg_base + 1),
                          opt3001_model_on_config_lo);

    /* OVF, CRF, FH and FL are R in Figure 24: only the part changes them */
    hw_model_set_writable_mask(&m->hw, cfg_base, OPT_CFG_HI_WRITABLE);
    hw_model_set_writable_mask(&m->hw, (uint8_t)(cfg_base + 1),
                               OPT_CFG_LO_WRITABLE);

    /* p21: "Configuration Register (offset = 01h) [reset = C810h]": auto
     * range, 800 ms, shutdown, latched */
    m->hw.reset_values[cfg_base]     = 0xC8u;
    m->hw.reset_values[cfg_base + 1] = 0x10u;

    /* Default simulated lux: 500.0 */
    m->true_lux = 500.0f;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_OPT3001_H */
