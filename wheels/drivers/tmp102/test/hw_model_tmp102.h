// SPDX-License-Identifier: MIT
/*
 * hw_model_tmp102.h — TMP102 hardware register model
 *
 * Models TI TMP102 register behavior:
 * - Temperature register 0x00 (16-bit, read-only, big-endian, 12-bit
 *   in bits[15:4], 0.0625 C/LSB)
 * - Config register 0x01 (16-bit, read-write, SD bit = byte0[0])
 * - T_LOW register 0x02 (16-bit, read-write, alert threshold)
 * - T_HIGH register 0x03 (16-bit, read-write, alert threshold)
 *
 * No chip ID register — TMP102 identifies via config register defaults.
 * All registers are 16-bit big-endian accessed via 2-byte I2C ops.
 *
 * Uses same slot-mapping as ADS1115: reg N -> byte slots [2*N, 2*N+1].
 */

#ifndef HW_MODEL_TMP102_H
#define HW_MODEL_TMP102_H

#include "hw_model.h"

/* --- TMP102 register addresses --- */
#define TMP102_REG_TEMP    0x00u
#define TMP102_REG_CONFIG  0x01u
#define TMP102_REG_TLOW    0x02u
#define TMP102_REG_THIGH   0x03u

/* Internal storage: each pointer register maps to 2 byte slots */
#define TMP_SLOT(reg) ((uint8_t)((reg) * 2))

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    float            true_temp_c;     /* simulated temperature */
} tmp102_model_t;

/* Convert Celsius to 12-bit register value (bits[15:4]). Table 6-2 gives
 * 128 C as 7FFh, so the format saturates at its ends. */
static void tmp102_c_to_raw16(float temp_c, uint8_t *msb, uint8_t *lsb)
{
    float scaled = temp_c / 0.0625f;
    int16_t raw;

    if (scaled >= 2047.0f)
        raw = 2047;
    else if (scaled <= -2048.0f)
        raw = -2048;
    else
        raw = (int16_t)scaled;

    uint16_t val = (uint16_t)((uint16_t)raw << 4);
    *msb = (uint8_t)(val >> 8);
    *lsb = (uint8_t)(val & 0xFF);
}

/* Update temperature data register from simulated temperature */
static void tmp102_model_update_temp(tmp102_model_t *m)
{
    uint8_t msb, lsb;
    tmp102_c_to_raw16(m->true_temp_c, &msb, &lsb);
    uint8_t slot = TMP_SLOT(TMP102_REG_TEMP);
    m->hw.regs[slot]     = msb;
    m->hw.regs[slot + 1] = lsb;
}

/*
 * Custom I2C callbacks for TMP102.
 * The TMP102 uses 16-bit registers addressed by a pointer byte.
 * read_regs(ctx, addr, reg, buf, 2) reads the 16-bit register.
 * write_regs(ctx, addr, reg, buf, 2) writes the 16-bit register.
 */

__attribute__((unused))
static bool tmp102_model_write_reg(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t val)
{
    (void)dev_addr; (void)ctx; (void)reg; (void)val;
    /* TMP102 driver does not use single-byte writes */
    return true;
}

__attribute__((unused))
static bool tmp102_model_write_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, const uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    tmp102_model_t *m = (tmp102_model_t *)ctx;

    if (len != 2 || reg > 3) return true;

    uint8_t slot = TMP_SLOT(reg);
    uint8_t mask0 = m->hw.writable_mask[slot];
    uint8_t mask1 = m->hw.writable_mask[slot + 1];
    m->hw.regs[slot]     = (uint8_t)((m->hw.regs[slot]     & (uint8_t)~mask0) | (buf[0] & mask0));
    m->hw.regs[slot + 1] = (uint8_t)((m->hw.regs[slot + 1] & (uint8_t)~mask1) | (buf[1] & mask1));

    return true;
}

__attribute__((unused))
static bool tmp102_model_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t *buf,
                                    uint16_t len)
{
    (void)dev_addr;
    tmp102_model_t *m = (tmp102_model_t *)ctx;

    if (reg > 3) {
        memset(buf, 0xFF, len);
        return true;
    }

    /* Always return fresh temperature */
    if (reg == TMP102_REG_TEMP)
        tmp102_model_update_temp(m);

    uint8_t slot = TMP_SLOT(reg);
    for (uint16_t i = 0; i < len && (slot + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->hw.regs[slot + i];
    }
    return true;
}

static void tmp102_model_init(tmp102_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Temperature register (read-only) */
    hw_model_set_readonly(&m->hw, TMP_SLOT(TMP102_REG_TEMP));
    hw_model_set_readonly(&m->hw, TMP_SLOT(TMP102_REG_TEMP) + 1);

    /* Config register default: 0x60A0 (12-bit mode, 4 Hz, continuous) */
    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    m->hw.reset_values[cfg_slot]     = 0x60;
    m->hw.reset_values[cfg_slot + 1] = 0xA0;

    /* T_LOW default: 75 C = (75/0.0625)<<4 = 0x4B00 */
    uint8_t tlo_slot = TMP_SLOT(TMP102_REG_TLOW);
    m->hw.reset_values[tlo_slot]     = 0x4B;
    m->hw.reset_values[tlo_slot + 1] = 0x00;

    /* T_HIGH default: 80 C = (80/0.0625)<<4 = 0x5000 */
    uint8_t thi_slot = TMP_SLOT(TMP102_REG_THIGH);
    m->hw.reset_values[thi_slot]     = 0x50;
    m->hw.reset_values[thi_slot + 1] = 0x00;

    /* Default temperature: 25.0 C */
    m->true_temp_c = 25.0f;

    hw_model_reset(&m->hw);
    tmp102_model_update_temp(m);
}

static void tmp102_model_set_temperature(tmp102_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

#endif /* HW_MODEL_TMP102_H */
