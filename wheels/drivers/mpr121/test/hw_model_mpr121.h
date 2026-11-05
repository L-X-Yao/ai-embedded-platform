// SPDX-License-Identifier: MIT
/*
 * hw_model_mpr121.h — datasheet-accurate register model for MPR121
 *
 * Models NXP MPR121 behavior:
 * - Register-based I2C interface at 0x5A-0x5D
 * - 12 capacitive touch electrodes with 10-bit filtered data
 * - Touch status: regs 0x00-0x01, ELE0-11 in bits 11:0, E12TS (proximity) in
 *   bit 12, OVCF in bit 15
 * - Electrode filtered data: regs 0x04-0x1D (2 bytes each)
 * - Touch/release thresholds: regs 0x41-0x5A
 * - ECR (0x5E): electrode configuration
 * - Soft reset: write 0x63 to 0x80 → ECR becomes 0x00
 *
 * Uses model-based I2C callbacks for round-trip verification.
 */
#ifndef HW_MODEL_MPR121_H
#define HW_MODEL_MPR121_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../mpr121.h"

#define MPR121_MODEL_ELECTRODE_COUNT  12

typedef struct {
    uint16_t touch_status;       /* ELE0-11, plus E12TS at bit 12 */
    bool     ovcf;               /* over current flag, bit 7 of 0x01 */
    uint16_t electrode[MPR121_MODEL_ELECTRODE_COUNT];  /* 10-bit filtered data */
    uint8_t  touch_threshold[MPR121_MODEL_ELECTRODE_COUNT];
    uint8_t  release_threshold[MPR121_MODEL_ELECTRODE_COUNT];
    uint8_t  ecr;
    uint8_t  mhdr, nhdr, nclr, fdlr;
    uint8_t  mhdf, nhdf, nclf, fdlf;
    bool     reset_pending;      /* set by soft-reset write */
    int      write_count;
    int      read_count;
    int      delay_count;
} mpr121_model_t;

static void mpr121_model_reset(mpr121_model_t *m)
{
    memset(m, 0, sizeof(*m));
    /* After POR / soft-reset, ECR is 0x00 (all electrodes stopped) */
    m->ecr = 0x00u;
    m->reset_pending = false;
}

/* Inject touch status bitmap (bit N = electrode N touched, bit 12 = E12TS) */
static void mpr121_model_inject_touch(mpr121_model_t *m, uint16_t status)
{
    m->touch_status = status & 0x1FFFu;
}

/* Inject over current on REXT. Rev 4 p.11: OVCF is set, the part goes to Stop
 * Mode, and the ExTS bits, output registers and ECR D5-D0 are cleared. */
static void mpr121_model_inject_overcurrent(mpr121_model_t *m)
{
    m->ovcf = true;
    m->touch_status = 0;
    memset(m->electrode, 0, sizeof(m->electrode));
    m->ecr &= 0xC0u;
}

/* Inject 10-bit electrode filtered data for a channel */
static void mpr121_model_inject_electrode(mpr121_model_t *m,
                                          uint8_t channel, uint16_t value)
{
    if (channel < MPR121_MODEL_ELECTRODE_COUNT) {
        m->electrode[channel] = value & 0x03FFu;
    }
}

/* Build register file from model state */
static void mpr121_model_build_regs(const mpr121_model_t *m,
                                    uint8_t *regs, uint16_t len)
{
    if (len < 256) return;
    memset(regs, 0, len);

    /* Touch status: 0x00 = low byte, 0x01 = high byte */
    regs[0x00] = (uint8_t)(m->touch_status & 0xFFu);
    regs[0x01] = (uint8_t)(((m->touch_status >> 8) & 0x1Fu)
                           | (m->ovcf ? 0x80u : 0x00u));

    /* Electrode filtered data: 0x04-0x1D, 2 bytes each (LSB first) */
    for (uint8_t ch = 0; ch < MPR121_MODEL_ELECTRODE_COUNT; ch++) {
        uint8_t base = (uint8_t)(0x04u + ch * 2u);
        regs[base]     = (uint8_t)(m->electrode[ch] & 0xFFu);
        regs[base + 1u] = (uint8_t)((m->electrode[ch] >> 8) & 0x03u);
    }

    /* Baseline filtering */
    regs[0x2B] = m->mhdr;
    regs[0x2C] = m->nhdr;
    regs[0x2D] = m->nclr;
    regs[0x2E] = m->fdlr;
    regs[0x2F] = m->mhdf;
    regs[0x30] = m->nhdf;
    regs[0x31] = m->nclf;
    regs[0x32] = m->fdlf;

    /* Touch/release thresholds */
    for (uint8_t ch = 0; ch < MPR121_MODEL_ELECTRODE_COUNT; ch++) {
        uint8_t tth_reg = (uint8_t)(0x41u + ch * 2u);
        uint8_t rth_reg = (uint8_t)(0x41u + ch * 2u + 1u);
        regs[tth_reg] = m->touch_threshold[ch];
        regs[rth_reg] = m->release_threshold[ch];
    }

    /* ECR */
    regs[0x5E] = m->ecr;
}

/* I2C write callback backed by model */
__attribute__((unused))
static bool mpr121_model_write_reg(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    mpr121_model_t *m = (mpr121_model_t *)ctx;
    m->write_count++;

    /* Soft reset: write 0x63 to 0x80 */
    if (reg == 0x80u && val == 0x63u) {
        m->ecr = 0x00u;
        m->ovcf = false;
        memset(m->touch_threshold, 0, sizeof(m->touch_threshold));
        memset(m->release_threshold, 0, sizeof(m->release_threshold));
        m->mhdr = 0; m->nhdr = 0; m->nclr = 0; m->fdlr = 0;
        m->mhdf = 0; m->nhdf = 0; m->nclf = 0; m->fdlf = 0;
        m->reset_pending = true;
        return true;
    }

    /* OVCF: writing 1 clears it; the ExTS bits are read-only */
    if (reg == 0x01u) {
        if ((val & 0x80u) != 0u) m->ovcf = false;
        return true;
    }

    /* ECR write. While OVCF is 1, a write to enter Run Mode is discarded. */
    if (reg == 0x5Eu) {
        if (m->ovcf && (val & 0x3Fu) != 0u) return true;
        m->ecr = val;
        return true;
    }

    /* Baseline filtering registers */
    if (reg == 0x2Bu) { m->mhdr = val; return true; }
    if (reg == 0x2Cu) { m->nhdr = val; return true; }
    if (reg == 0x2Du) { m->nclr = val; return true; }
    if (reg == 0x2Eu) { m->fdlr = val; return true; }
    if (reg == 0x2Fu) { m->mhdf = val; return true; }
    if (reg == 0x30u) { m->nhdf = val; return true; }
    if (reg == 0x31u) { m->nclf = val; return true; }
    if (reg == 0x32u) { m->fdlf = val; return true; }

    /* Touch/release threshold writes: 0x41-0x5A */
    if (reg >= 0x41u && reg <= 0x58u) {
        uint8_t offset = (uint8_t)(reg - 0x41u);
        uint8_t ch = (uint8_t)(offset / 2u);
        if (ch < MPR121_MODEL_ELECTRODE_COUNT) {
            if ((offset & 1u) == 0u) {
                m->touch_threshold[ch] = val;
            } else {
                m->release_threshold[ch] = val;
            }
        }
        return true;
    }

    return true;
}

/* I2C read callback backed by model */
__attribute__((unused))
static bool mpr121_model_read_regs(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    mpr121_model_t *m = (mpr121_model_t *)ctx;
    m->read_count++;

    uint8_t regs[256];
    mpr121_model_build_regs(m, regs, 256);

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < 256) ? regs[idx] : 0xFFu;
    }
    return true;
}

/* Delay callback — just counts calls */
__attribute__((unused))
static void mpr121_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ms;
    mpr121_model_t *m = (mpr121_model_t *)ctx;
    m->delay_count++;
}

#endif /* HW_MODEL_MPR121_H */
