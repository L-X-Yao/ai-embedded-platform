// SPDX-License-Identifier: MIT
/*
 * hw_model_ads1115.h — ADS1115 hardware register model
 *
 * Models TI ADS1115 register behavior:
 * - Only 4 registers (16-bit each): Conversion (0x00), Config (0x01),
 *   Lo_thresh (0x02), Hi_thresh (0x03)
 * - Config register bit 15: OS (operational status / start conversion)
 * - Conversion register: read-only, populated when conversion completes
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

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_adc;        /* simulated conversion result */
    int              conversion_count;
} ads1115_model_t;

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

    /* If writing config with OS=1 (bit 15 = buf[0] bit 7), start conversion */
    if (reg == ADS1115_REG_CONFIG && (buf[0] & 0x80)) {
        /* Conversion result: write true_adc into conversion register */
        uint8_t conv_slot = ADS_SLOT(ADS1115_REG_CONVERSION);
        m->hw.regs[conv_slot]     = (uint8_t)((uint16_t)m->true_adc >> 8);
        m->hw.regs[conv_slot + 1] = (uint8_t)((uint16_t)m->true_adc & 0xFF);
        /* Set OS=1 in config to indicate conversion complete */
        m->hw.regs[slot] |= 0x80;
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

    /* Default simulated ADC: 1000 counts (~0.125V at PGA=2048) */
    m->true_adc = 1000;

    hw_model_reset(&m->hw);
}

static void ads1115_model_set_adc(ads1115_model_t *m, int16_t raw)
{
    m->true_adc = raw;
}

#endif /* HW_MODEL_ADS1115_H */
