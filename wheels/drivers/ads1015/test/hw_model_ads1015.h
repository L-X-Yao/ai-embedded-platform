// SPDX-License-Identifier: MIT
/*
 * hw_model_ads1015.h — ADS1015 hardware register model
 *
 * Models TI ADS1015 register behavior:
 * - Only 4 registers (16-bit each): Conversion (0x00), Config (0x01),
 *   Lo_thresh (0x02), Hi_thresh (0x03)
 * - Config register bit 15: OS (operational status / start conversion)
 * - Conversion register: read-only, populated when conversion completes
 * - 12-bit result left-justified in 16-bit conversion register
 *
 * Identical register layout to ADS1115, but 12-bit ADC (vs 16-bit).
 * The model stores each 16-bit register as two consecutive 8-bit
 * slots: MSB at even address, LSB at odd address.
 */

#ifndef HW_MODEL_ADS1015_H
#define HW_MODEL_ADS1015_H

#include "hw_model.h"

/* --- ADS1015 register addresses --- */
#define ADS1015_REG_CONVERSION  0x00u
#define ADS1015_REG_CONFIG      0x01u
#define ADS1015_REG_LO_THRESH   0x02u
#define ADS1015_REG_HI_THRESH   0x03u

/* Internal storage: each pointer register maps to 2 byte slots */
#define ADS1015_SLOT(reg) ((uint8_t)((reg) * 2))

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_adc_12bit;   /* simulated 12-bit conversion result (-2048..+2047) */
    int              conversion_count;
} ads1015_model_t;

/*
 * Custom I2C callbacks for ADS1015.
 * The ADS1015 uses 16-bit registers addressed by a single pointer byte.
 * read_regs(ctx, addr, reg, buf, 2) reads the 16-bit register.
 * write_regs(ctx, addr, reg, buf, 2) writes the 16-bit register.
 */

__attribute__((unused))
static bool ads1015_model_write_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, const uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    ads1015_model_t *m = (ads1015_model_t *)ctx;

    if (len != 2 || reg > 3) return true;

    uint8_t slot = ADS1015_SLOT(reg);
    uint8_t mask0 = m->hw.writable_mask[slot];
    uint8_t mask1 = m->hw.writable_mask[slot + 1];
    m->hw.regs[slot]     = (uint8_t)((m->hw.regs[slot]     & (uint8_t)~mask0) | (buf[0] & mask0));
    m->hw.regs[slot + 1] = (uint8_t)((m->hw.regs[slot + 1] & (uint8_t)~mask1) | (buf[1] & mask1));

    /* If writing config with OS=1 (bit 15 = buf[0] bit 7), start conversion */
    if (reg == ADS1015_REG_CONFIG && (buf[0] & 0x80)) {
        /* 12-bit result is left-justified in 16-bit register (bits [15:4]) */
        uint16_t conv = (uint16_t)((uint16_t)m->true_adc_12bit << 4);
        uint8_t conv_slot = ADS1015_SLOT(ADS1015_REG_CONVERSION);
        m->hw.regs[conv_slot]     = (uint8_t)(conv >> 8);
        m->hw.regs[conv_slot + 1] = (uint8_t)(conv & 0xFF);
        /* Set OS=1 in config to indicate conversion complete */
        m->hw.regs[slot] |= 0x80;
        m->conversion_count++;
    }

    return true;
}

__attribute__((unused))
static bool ads1015_model_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t *buf,
                                    uint16_t len)
{
    (void)dev_addr;
    ads1015_model_t *m = (ads1015_model_t *)ctx;

    if (reg > 3) {
        memset(buf, 0xFF, len);
        return true;
    }

    uint8_t slot = ADS1015_SLOT(reg);
    for (uint16_t i = 0; i < len && (slot + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->hw.regs[slot + i];
    }
    return true;
}

static void ads1015_model_init(ads1015_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Conversion register (0x00): read-only */
    hw_model_set_readonly(&m->hw, ADS1015_SLOT(ADS1015_REG_CONVERSION));
    hw_model_set_readonly(&m->hw, ADS1015_SLOT(ADS1015_REG_CONVERSION) + 1);

    /* Config register (0x01): default 0x8583 (same as ADS1115 power-on reset) */
    uint8_t cfg_slot = ADS1015_SLOT(ADS1015_REG_CONFIG);
    m->hw.reset_values[cfg_slot]     = 0x85;
    m->hw.reset_values[cfg_slot + 1] = 0x83;

    /* Default simulated ADC: 500 counts (within 12-bit range -2048..+2047) */
    m->true_adc_12bit = 500;

    hw_model_reset(&m->hw);
}

/* Helper: set simulated 12-bit ADC value (-2048..+2047) */
static void ads1015_model_set_adc(ads1015_model_t *m, int16_t raw_12bit)
{
    m->true_adc_12bit = raw_12bit;
}

#endif /* HW_MODEL_ADS1015_H */
