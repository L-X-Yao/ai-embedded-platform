// SPDX-License-Identifier: MIT
/*
 * ltr390 — Lite-On LTR-390UV UV and ambient light sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (LTR-390UV-01 datasheet):
 *   0x00  MAIN_CTRL    (R/W) — mode select [3], enable [1]
 *   0x04  ALS_UVS_MEAS_RATE (R/W) — resolution [6:4], measurement rate [2:0]
 *   0x05  ALS_UVS_GAIN (R/W) — gain [2:0]
 *   0x06  PART_ID      (R)   — part number [7:4]=0xB, revision [3:0]
 *   0x07  MAIN_STATUS  (R)   — power-on status, interrupt, data status
 *   0x0D  ALS_DATA_0   (R)   — ALS data byte 0 (LSB)
 *   0x0E  ALS_DATA_1   (R)   — ALS data byte 1
 *   0x0F  ALS_DATA_2   (R)   — ALS data byte 2 (bits [3:0] valid)
 *   0x10  UVS_DATA_0   (R)   — UVS data byte 0 (LSB)
 *   0x11  UVS_DATA_1   (R)   — UVS data byte 1
 *   0x12  UVS_DATA_2   (R)   — UVS data byte 2 (bits [3:0] valid)
 */

#include "ltr390.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_MAIN_CTRL      0x00u
#define REG_MEAS_RATE      0x04u
#define REG_GAIN           0x05u
#define REG_PART_ID        0x06u
#define REG_ALS_DATA_0     0x0Du
#define REG_UVS_DATA_0     0x10u

/* ---------- MAIN_CTRL bits ---------- */

#define MAIN_CTRL_ENABLE   (1u << 1)   /* bit 1: sensor enable */
#define MAIN_CTRL_UVS_MODE (1u << 3)   /* bit 3: 0=ALS, 1=UVS */

/* ---------- MEAS_RATE fields ---------- */

#define MEAS_RATE_100MS    0x02u       /* bits [2:0]: POR default, 010 */

/* ---------- masks ---------- */

#define DATA_20BIT_MASK    0x000FFFFFu

/* ---------- internal helpers ---------- */

static bool write_reg(const ltr390_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, LTR390_DEV_ADDR, reg, val);
}

static bool read_regs(const ltr390_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, LTR390_DEV_ADDR, reg, buf, len);
}

static bool gain_valid(ltr390_gain_t g)
{
    return g <= LTR390_GAIN_18;
}

static bool res_valid(ltr390_res_t r)
{
    return r <= LTR390_RES_13BIT;
}

static bool mode_valid(ltr390_mode_t m)
{
    return m <= LTR390_MODE_UVS;
}

/** Build MAIN_CTRL register value from mode and enable bit. */
static uint8_t build_main_ctrl(ltr390_mode_t mode)
{
    uint8_t val = MAIN_CTRL_ENABLE;
    if (mode == LTR390_MODE_UVS) {
        val |= MAIN_CTRL_UVS_MODE;
    }
    return val;
}

/** Read 3-byte 20-bit little-endian data starting at reg. */
static ltr390_err_t read_20bit(const ltr390_t *dev, uint8_t reg,
                               uint32_t *out)
{
    uint8_t buf[3] = {0, 0, 0};
    if (!read_regs(dev, reg, buf, 3)) {
        return LTR390_ERR_I2C;
    }
    *out = ((uint32_t)buf[2] << 16 |
            (uint32_t)buf[1] << 8  |
            (uint32_t)buf[0]) & DATA_20BIT_MASK;
    return LTR390_OK;
}

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static ltr390_err_t bring_up(ltr390_t *dev, const ltr390_cfg_t *cfg)
{
    /* Verify part ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_PART_ID, &id, 1)) {
        return LTR390_ERR_I2C;
    }
    if ((id & 0xF0u) != (LTR390_PART_ID_VAL & 0xF0u)) {
        return LTR390_ERR_ID;
    }

    /* Set gain */
    if (!write_reg(dev, REG_GAIN, (uint8_t)cfg->gain)) {
        return LTR390_ERR_I2C;
    }

    /* Set resolution (bits [6:4]). One write covers the whole register, so
     * measurement rate bits [2:0] are written with their POR default. */
    if (!write_reg(dev, REG_MEAS_RATE,
                   (uint8_t)(((uint8_t)cfg->resolution << 4) | MEAS_RATE_100MS))) {
        return LTR390_ERR_I2C;
    }

    /* Set mode and enable */
    dev->mode = cfg->mode;
    if (!write_reg(dev, REG_MAIN_CTRL, build_main_ctrl(dev->mode))) {
        return LTR390_ERR_I2C;
    }

    return LTR390_OK;
}

ltr390_err_t ltr390_init(ltr390_t *dev, const ltr390_i2c_t *i2c,
                         const ltr390_cfg_t *cfg)
{
    if (!dev) {
        return LTR390_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg) {
        return LTR390_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg) {
        return LTR390_ERR_PARAM;
    }
    if (!gain_valid(cfg->gain) || !res_valid(cfg->resolution) ||
        !mode_valid(cfg->mode)) {
        return LTR390_ERR_PARAM;
    }

    dev->i2c = i2c;

    ltr390_err_t err = bring_up(dev, cfg);
    if (err != LTR390_OK) {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return LTR390_OK;
}

ltr390_err_t ltr390_read_als(ltr390_t *dev, uint32_t *raw)
{
    if (!dev || !dev->i2c || !raw) {
        return LTR390_ERR_PARAM;
    }
    return read_20bit(dev, REG_ALS_DATA_0, raw);
}

ltr390_err_t ltr390_read_uvs(ltr390_t *dev, uint32_t *raw)
{
    if (!dev || !dev->i2c || !raw) {
        return LTR390_ERR_PARAM;
    }
    return read_20bit(dev, REG_UVS_DATA_0, raw);
}

ltr390_err_t ltr390_set_mode(ltr390_t *dev, ltr390_mode_t mode)
{
    if (!dev || !dev->i2c) {
        return LTR390_ERR_PARAM;
    }
    if (!mode_valid(mode)) {
        return LTR390_ERR_PARAM;
    }

    if (!write_reg(dev, REG_MAIN_CTRL, build_main_ctrl(mode))) {
        return LTR390_ERR_I2C;
    }
    dev->mode = mode;
    return LTR390_OK;
}
