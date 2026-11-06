// SPDX-License-Identifier: MIT
/*
 * hw_model_tmp102.h — TMP102 hardware register model
 *
 * Models TI TMP102 register behavior:
 * - Temperature register 0x00 (16-bit, read-only, big-endian, 12-bit
 *   in bits[15:4], 0.0625 C/LSB)
 * - Config register 0x01 (16-bit, SD bit = byte0[0]). R1, R0 and AL are
 *   read-only (p18, p19)
 * - T_LOW register 0x02 (16-bit, read-write, alert threshold)
 * - T_HIGH register 0x03 (16-bit, read-write, alert threshold)
 * - Conversions, from TI SBOS397I: the register reads 0 until the first
 *   result, 10 ms after power-up, then one result per CR period (p8, p15).
 *   Time passes only through tmp102_model_run_ms(). SD stops them once the
 *   current one is done, and clearing it starts them again (p15). In shutdown
 *   OS runs one conversion, reading 0 during it and 1 after (p15)
 * - Each result updates AL by T_HIGH, T_LOW, POL and the fault queue (p18,
 *   p19)
 *
 * Extended mode is not modelled: EM is stored and has no effect.
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

/* --- configuration bits --- */
#define TMP102_CFG1_OS     0x80u
#define TMP102_CFG1_SD     0x01u
#define TMP102_CFG1_POL    0x04u
#define TMP102_CFG2_AL     0x20u

#define TMP102_T_CONV_MS   10u

/* Internal storage: each pointer register maps to 2 byte slots */
#define TMP_SLOT(reg) ((uint8_t)((reg) * 2))

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    float            true_temp_c;     /* simulated temperature */
    uint32_t         period_age_ms;   /* since this conversion started */
    bool             converted;       /* this period's result is in */
    bool             one_shot;        /* an OS conversion is running */
    int              conversions;     /* results so far */
    bool             alert;           /* comparator state behind AL */
    int              faults_high;     /* consecutive results at or over T_HIGH */
    int              faults_low;      /* consecutive results under T_LOW */
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

static int16_t tmp102_model_reg16(const tmp102_model_t *m, uint8_t reg)
{
    uint8_t slot = TMP_SLOT(reg);
    return (int16_t)(uint16_t)((uint16_t)m->hw.regs[slot] << 8 |
                               m->hw.regs[slot + 1]);
}

/* One result reaches the register, and the comparator looks at it */
static void tmp102_model_finish_conversion(tmp102_model_t *m)
{
    static const int queue[4] = { 1, 2, 4, 6 };
    uint8_t slot = TMP_SLOT(TMP102_REG_CONFIG);
    uint8_t cfg1 = m->hw.regs[slot];
    int needed = queue[(cfg1 >> 3) & 0x03u];

    tmp102_model_update_temp(m);
    m->conversions++;

    int16_t temp = tmp102_model_reg16(m, TMP102_REG_TEMP);
    m->faults_high = temp >= tmp102_model_reg16(m, TMP102_REG_THIGH)
                         ? m->faults_high + 1 : 0;
    m->faults_low  = temp < tmp102_model_reg16(m, TMP102_REG_TLOW)
                         ? m->faults_low + 1 : 0;
    if (m->faults_high >= needed) m->alert = true;
    if (m->faults_low >= needed)  m->alert = false;

    /* POL 0: AL reads 1 until the alert, POL 1 inverts it */
    bool al = m->alert == ((cfg1 & TMP102_CFG1_POL) != 0);
    m->hw.regs[slot + 1] = al
        ? (uint8_t)(m->hw.regs[slot + 1] | TMP102_CFG2_AL)
        : (uint8_t)(m->hw.regs[slot + 1] & (uint8_t)~TMP102_CFG2_AL);
}

static uint32_t tmp102_model_period_ms(const tmp102_model_t *m)
{
    static const uint32_t period[4] = { 4000u, 1000u, 250u, 125u };
    return period[m->hw.regs[TMP_SLOT(TMP102_REG_CONFIG) + 1] >> 6];
}

/* One millisecond of the conversion engine */
static void tmp102_model_tick(tmp102_model_t *m)
{
    uint8_t slot = TMP_SLOT(TMP102_REG_CONFIG);
    bool sd = (m->hw.regs[slot] & TMP102_CFG1_SD) != 0;

    /* Shutdown waits for the conversion under way, if there is one */
    if (sd && !m->one_shot && m->converted) return;

    m->period_age_ms++;
    if (!m->converted && m->period_age_ms >= TMP102_T_CONV_MS) {
        tmp102_model_finish_conversion(m);
        m->converted = true;
        if (m->one_shot) {
            m->one_shot = false;
            m->hw.regs[slot] |= TMP102_CFG1_OS;
        }
    }
    if (!sd && m->period_age_ms >= tmp102_model_period_ms(m)) {
        m->period_age_ms = 0;
        m->converted = false;
    }
}

/* Time passes */
__attribute__((unused))
static void tmp102_model_run_ms(tmp102_model_t *m, uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) tmp102_model_tick(m);
}

static void tmp102_model_write_config(tmp102_model_t *m, const uint8_t *buf)
{
    uint8_t slot = TMP_SLOT(TMP102_REG_CONFIG);
    uint8_t mask1 = m->hw.writable_mask[slot];
    uint8_t mask2 = m->hw.writable_mask[slot + 1];
    bool was_sd = (m->hw.regs[slot] & TMP102_CFG1_SD) != 0;
    uint8_t os = m->hw.regs[slot] & TMP102_CFG1_OS;

    m->hw.regs[slot] = (uint8_t)((m->hw.regs[slot] & (uint8_t)~mask1) |
                                 (buf[0] & mask1) | os);
    m->hw.regs[slot + 1] = (uint8_t)((m->hw.regs[slot + 1] & (uint8_t)~mask2) |
                                     (buf[1] & mask2));
    bool sd = (m->hw.regs[slot] & TMP102_CFG1_SD) != 0;

    if (sd && (buf[0] & TMP102_CFG1_OS)) {
        m->hw.regs[slot] &= (uint8_t)~TMP102_CFG1_OS;
        m->one_shot = true;
        m->period_age_ms = 0;
        m->converted = false;
    } else if (was_sd && !sd) {
        m->period_age_ms = 0;
        m->converted = false;
    }
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

    if (reg == TMP102_REG_CONFIG) {
        tmp102_model_write_config(m, buf);
        return true;
    }

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

    /* Config register default: 0x60A0 (12-bit mode, 4 Hz, continuous).
     * OS R1 R0 F1 F0 POL TM SD, then CR1 CR0 AL EM 0 0 0 0. OS is handled
     * in tmp102_model_write_config(). */
    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    m->hw.reset_values[cfg_slot]     = 0x60;
    m->hw.reset_values[cfg_slot + 1] = 0xA0;
    hw_model_set_writable_mask(&m->hw, cfg_slot, 0x1F);
    hw_model_set_writable_mask(&m->hw, (uint8_t)(cfg_slot + 1), 0xD0);

    /* T_LOW default: 75 C = (75/0.0625)<<4 = 0x4B00 */
    uint8_t tlo_slot = TMP_SLOT(TMP102_REG_TLOW);
    m->hw.reset_values[tlo_slot]     = 0x4B;
    m->hw.reset_values[tlo_slot + 1] = 0x00;
    hw_model_set_writable_mask(&m->hw, (uint8_t)(tlo_slot + 1), 0xF0);

    /* T_HIGH default: 80 C = (80/0.0625)<<4 = 0x5000 */
    uint8_t thi_slot = TMP_SLOT(TMP102_REG_THIGH);
    m->hw.reset_values[thi_slot]     = 0x50;
    m->hw.reset_values[thi_slot + 1] = 0x00;
    hw_model_set_writable_mask(&m->hw, (uint8_t)(thi_slot + 1), 0xF0);

    /* Default temperature: 25.0 C */
    m->true_temp_c = 25.0f;

    /* The temperature register stays 0 until the first result */
    hw_model_reset(&m->hw);
}

static void tmp102_model_set_temperature(tmp102_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

#endif /* HW_MODEL_TMP102_H */
