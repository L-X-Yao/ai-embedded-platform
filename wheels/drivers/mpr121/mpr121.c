// SPDX-License-Identifier: MIT
/*
 * mpr121 — NXP MPR121 12-channel capacitive touch sensor (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (NXP MPR121 datasheet):
 *   0x00-0x01  Touch status (bits 11:0 electrodes, 12 proximity, 15 OVCF)
 *   0x04-0x1D  Electrode 0-11 filtered data (2 bytes each, 10-bit)
 *   0x2B-0x32  Baseline filtering control (MHD/NHD/NCL/FDL)
 *   0x41-0x5A  Touch/release thresholds (2 bytes per electrode)
 *   0x5E       ECR (electrode configuration register)
 *   0x80       Soft reset (write 0x63)
 *
 * Soft-reset sequence: write 0x63 to reg 0x80, wait 1ms.
 * After reset ECR reads 0x00 (all electrodes stopped).
 * No chip ID register exists; verify via ECR==0x00 after reset.
 */

#include "mpr121.h"
#include "contract.h"
#include <string.h>

/* ---------- internal constants ---------- */

#define SOFT_RESET_VALUE   0x63u
#define RESET_DELAY_MS     1u
#define ELECTRODE_DATA_LEN 2u     /* 2 bytes per electrode (10-bit) */
#define ELECTRODE_DATA_MASK 0x03FFu  /* 10-bit mask */
#define TOUCH_BITMAP_MASK  0x0FFFu   /* ELE0-11 in the touch status word */
#define TOUCH_STATUS_OVCF  0x8000u   /* over current flag, bit 7 of 0x01 */

/* Baseline filtering defaults (from datasheet application note) */
#define DEFAULT_MHDR  0x01u
#define DEFAULT_NHDR  0x01u
#define DEFAULT_NCLR  0x0Eu
#define DEFAULT_FDLR  0x00u
#define DEFAULT_MHDF  0x01u
#define DEFAULT_NHDF  0x05u
#define DEFAULT_NCLF  0x01u
#define DEFAULT_FDLF  0x00u

/* ---------- internal helpers ---------- */

static mpr121_err_t write_reg(const mpr121_t *dev, uint8_t reg, uint8_t val)
{
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, reg, val))
        return MPR121_ERR_I2C;
    return MPR121_OK;
}

static mpr121_err_t read_regs(const mpr121_t *dev, uint8_t reg,
                              uint8_t *buf, uint16_t len)
{
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, len))
        return MPR121_ERR_I2C;
    return MPR121_OK;
}

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static mpr121_err_t bring_up(const mpr121_t *dev, const mpr121_i2c_t *i2c,
                             const mpr121_cfg_t *cfg)
{
    /* Soft reset: write 0x63 to register 0x80 */
    mpr121_err_t err = write_reg(dev, MPR121_REG_SOFT_RESET, SOFT_RESET_VALUE);
    if (err != MPR121_OK) return err;

    i2c->delay_ms(i2c->ctx, RESET_DELAY_MS);

    /* Verify reset: ECR must read 0x00 after soft reset */
    uint8_t ecr_val = 0xFFu;
    err = read_regs(dev, MPR121_REG_ECR, &ecr_val, 1);
    if (err != MPR121_OK) return err;
    if (ecr_val != 0x00u) return MPR121_ERR_I2C;

    /* Stop electrodes before configuration (ECR = 0x00, already done by reset) */

    /* Baseline filtering control */
    err = write_reg(dev, MPR121_REG_MHDR, DEFAULT_MHDR);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_NHDR, DEFAULT_NHDR);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_NCLR, DEFAULT_NCLR);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_FDLR, DEFAULT_FDLR);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_MHDF, DEFAULT_MHDF);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_NHDF, DEFAULT_NHDF);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_NCLF, DEFAULT_NCLF);
    if (err != MPR121_OK) return err;
    err = write_reg(dev, MPR121_REG_FDLF, DEFAULT_FDLF);
    if (err != MPR121_OK) return err;

    /* Set touch/release thresholds for all 12 electrodes */
    for (uint8_t ch = 0; ch < MPR121_CHANNEL_COUNT; ch++) {
        uint8_t tth_reg = (uint8_t)(MPR121_REG_E0TTH + ch * 2u);
        uint8_t rth_reg = (uint8_t)(MPR121_REG_E0TTH + ch * 2u + 1u);

        err = write_reg(dev, tth_reg, cfg->touch_threshold);
        if (err != MPR121_OK) return err;
        err = write_reg(dev, rth_reg, cfg->release_threshold);
        if (err != MPR121_OK) return err;
    }

    /* Enable electrodes via ECR — must be last */
    return write_reg(dev, MPR121_REG_ECR, cfg->ecr);
}

mpr121_err_t mpr121_init(mpr121_t *dev,
                         const mpr121_i2c_t *i2c,
                         const mpr121_cfg_t *cfg)
{
    if (!dev) return MPR121_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg) return MPR121_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return MPR121_ERR_PARAM;
    if (!i2c->delay_ms) return MPR121_ERR_PARAM;

    dev->i2c  = i2c;
    dev->addr = MPR121_DEV_ADDR;

    mpr121_err_t err = bring_up(dev, i2c, cfg);
    if (err != MPR121_OK) {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return MPR121_OK;
}

mpr121_err_t mpr121_read_touched(mpr121_t *dev, uint16_t *status)
{
    if (!dev || !dev->i2c || !status) return MPR121_ERR_PARAM;

    uint8_t buf[2];
    mpr121_err_t err = read_regs(dev, MPR121_REG_TOUCH_STATUS_L, buf, 2);
    if (err != MPR121_OK) return err;

    /* Touch status: low byte at 0x00, high byte at 0x01 */
    uint16_t raw = (uint16_t)((uint16_t)buf[1] << 8 | (uint16_t)buf[0]);
    *status = (uint16_t)(raw & TOUCH_BITMAP_MASK);

    if (raw & TOUCH_STATUS_OVCF) return MPR121_ERR_OVERCURRENT;

    return MPR121_OK;
}

mpr121_err_t mpr121_read_electrode(mpr121_t *dev, uint8_t channel,
                                   uint16_t *value)
{
    if (!dev || !dev->i2c || !value) return MPR121_ERR_PARAM;
    if (channel >= MPR121_CHANNEL_COUNT) return MPR121_ERR_PARAM;

    /* Each electrode has 2 bytes starting at 0x04, stride = 2 */
    uint8_t reg = (uint8_t)(MPR121_REG_EFD0_LB + channel * ELECTRODE_DATA_LEN);

    uint8_t buf[2];
    mpr121_err_t err = read_regs(dev, reg, buf, ELECTRODE_DATA_LEN);
    if (err != MPR121_OK) return err;

    /* Low byte first, high byte second; only 10 bits valid */
    *value = (uint16_t)((unsigned)((uint16_t)buf[1] << 8 | (uint16_t)buf[0])
                        & ELECTRODE_DATA_MASK);

    return MPR121_OK;
}
