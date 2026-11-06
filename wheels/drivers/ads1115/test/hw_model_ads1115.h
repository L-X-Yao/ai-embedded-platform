// SPDX-License-Identifier: MIT
/*
 * hw_model_ads1115.h — ADS1115 hardware register model
 *
 * Models TI ADS1115 register behavior:
 * - Only 4 registers (16-bit each): Conversion (0x00), Config (0x01),
 *   Lo_thresh (0x02), Hi_thresh (0x03)
 * - Config register bit 15: OS (operational status / start conversion).
 *   Only the model drives it: writing 0 has no effect (p.25), writing 1
 *   starts a conversion from power-down and has no effect while one runs
 *   (p.18). It reads 0 during the conversion and 1 otherwise.
 * - A conversion lasts 1 / DR (p.15) after about 25 us of power-up (p.18).
 *   Time moves with register reads and delay_ms calls.
 * - Conversion register: read-only, keeps the last result until the
 *   running conversion ends (p.24)
 * - All registers are 16-bit big-endian, accessed via 2-byte I2C ops
 *
 * Note: ADS1115 uses write_regs for 16-bit register writes.
 * The hw_model stores each 16-bit register as two consecutive 8-bit
 * slots: MSB at even address, LSB at odd address.
 * E.g. config reg 0x01 -> regs[2]=MSB, regs[3]=LSB.
 */

#ifndef HW_MODEL_ADS1115_H
#define HW_MODEL_ADS1115_H

#include "hw_model.h"

/* --- ADS1115 register addresses --- */
#define ADS1115_REG_CONVERSION  0x00u
#define ADS1115_REG_CONFIG      0x01u
#define ADS1115_REG_LO_THRESH   0x02u
#define ADS1115_REG_HI_THRESH   0x03u

/* Internal storage: each pointer register maps to 2 byte slots */
#define ADS_SLOT(reg) ((uint8_t)((reg) * 2))

/* p.18 "the device powers up in approximately 25 us" */
#define ADS1115_POWER_UP_US     25u

/* A register read is 45 clocks: 450 us on a 100 kHz bus. */
#define ADS1115_BUS_READ_US     450u

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_adc;        /* simulated conversion result */
    int              conversion_count; /* conversions started */
    bool             converting;      /* OS reads 0 while this is set */
    uint32_t         now_us;          /* model clock */
    uint32_t         done_us;         /* end of the running conversion */
    uint32_t         slow_percent;    /* p.5 data rate variation, 0..10 */
} ads1115_model_t;

/* Moves the clock and ends a conversion whose time is up. */
static void ads1115_model__advance(ads1115_model_t *m, uint32_t us)
{
    m->now_us += us;
    if (m->converting && m->now_us >= m->done_us) {
        uint8_t conv_slot = ADS_SLOT(ADS1115_REG_CONVERSION);
        m->hw.regs[conv_slot]     = (uint8_t)((uint16_t)m->true_adc >> 8);
        m->hw.regs[conv_slot + 1] = (uint8_t)((uint16_t)m->true_adc & 0xFF);
        m->hw.regs[ADS_SLOT(ADS1115_REG_CONFIG)] |= 0x80;
        m->converting = false;
    }
}

/* p.15 "the conversion time is equal to 1 / DR", DR[2:0] from p.26 */
static uint32_t ads1115_model__conversion_us(const ads1115_model_t *m,
                                             uint8_t dr)
{
    static const uint32_t sps[8] = {8, 16, 32, 64, 128, 250, 475, 860};
    return ADS1115_POWER_UP_US +
           100000000u / (sps[dr & 7u] * (100u - m->slow_percent));
}

/*
 * Custom I2C callbacks for ADS1115.
 * The ADS1115 uses 16-bit registers addressed by a single pointer byte.
 * read_regs(ctx, addr, reg, buf, 2) reads the 16-bit register.
 * write_regs(ctx, addr, reg, buf, 2) writes the 16-bit register.
 */

__attribute__((unused))
static bool ads1115_model_write_regs(void *ctx, uint8_t dev_addr,
                                      uint8_t reg, const uint8_t *buf,
                                      uint16_t len)
{
    (void)dev_addr;
    ads1115_model_t *m = (ads1115_model_t *)ctx;

    if (len != 2 || reg > 3) return true;

    uint8_t slot = ADS_SLOT(reg);
    uint8_t mask0 = m->hw.writable_mask[slot];
    uint8_t mask1 = m->hw.writable_mask[slot + 1];
    m->hw.regs[slot]     = (uint8_t)((m->hw.regs[slot]     & (uint8_t)~mask0) | (buf[0] & mask0));
    m->hw.regs[slot + 1] = (uint8_t)((m->hw.regs[slot + 1] & (uint8_t)~mask1) | (buf[1] & mask1));

    /* p.25 OS "can only be written when in power-down state and has no
     * effect when a conversion is ongoing". p.26 MODE = 1 is single-shot
     * or power-down. */
    if (reg == ADS1115_REG_CONFIG && (buf[0] & 0x80) && (buf[0] & 0x01) &&
        !m->converting) {
        m->hw.regs[slot] &= (uint8_t)~0x80u;
        m->converting = true;
        m->done_us = m->now_us +
                     ads1115_model__conversion_us(m, (uint8_t)(buf[1] >> 5));
        m->conversion_count++;
    }

    return true;
}

__attribute__((unused))
static bool ads1115_model_read_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    ads1115_model_t *m = (ads1115_model_t *)ctx;

    ads1115_model__advance(m, ADS1115_BUS_READ_US);

    if (reg > 3) {
        memset(buf, 0xFF, len);
        return true;
    }

    uint8_t slot = ADS_SLOT(reg);
    for (uint16_t i = 0; i < len && (slot + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->hw.regs[slot + i];
    }
    return true;
}

__attribute__((unused))
static void ads1115_model_delay_ms(void *ctx, uint32_t ms)
{
    ads1115_model__advance((ads1115_model_t *)ctx, ms * 1000u);
}

static void ads1115_model_init(ads1115_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Conversion register (0x00): read-only */
    hw_model_set_readonly(&m->hw, ADS_SLOT(ADS1115_REG_CONVERSION));
    hw_model_set_readonly(&m->hw, ADS_SLOT(ADS1115_REG_CONVERSION) + 1);

    /* Config register (0x01): default 0x8583 (ADS1115 power-on reset) */
    uint8_t cfg_slot = ADS_SLOT(ADS1115_REG_CONFIG);
    m->hw.reset_values[cfg_slot]     = 0x85;
    m->hw.reset_values[cfg_slot + 1] = 0x83;

    /* OS is status: p.25 "When writing: 0b : No effect" */
    hw_model_set_writable_mask(&m->hw, cfg_slot, 0x7F);

    /* p.27 Lo_thresh [reset = 8000h], Hi_thresh [reset = 7FFFh] */
    uint8_t lo_slot = ADS_SLOT(ADS1115_REG_LO_THRESH);
    uint8_t hi_slot = ADS_SLOT(ADS1115_REG_HI_THRESH);
    m->hw.reset_values[lo_slot]     = 0x80;
    m->hw.reset_values[lo_slot + 1] = 0x00;
    m->hw.reset_values[hi_slot]     = 0x7F;
    m->hw.reset_values[hi_slot + 1] = 0xFF;

    /* Default simulated ADC: 1000 counts (62.5 mV at PGA=2048, p.15
     * gives 62.5 uV per count at +-2.048 V) */
    m->true_adc = 1000;

    hw_model_reset(&m->hw);
}

static void ads1115_model_set_adc(ads1115_model_t *m, int16_t raw)
{
    m->true_adc = raw;
}

#endif /* HW_MODEL_ADS1115_H */
