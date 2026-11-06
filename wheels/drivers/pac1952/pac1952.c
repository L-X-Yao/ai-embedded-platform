// SPDX-License-Identifier: MIT
/*
 * pac1952 — Microchip PAC1952 dual-channel I2C power/energy monitor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (key registers, all big-endian):
 *   0x00 REFRESH (Send Byte command, no data), 0x01 CTRL, 0x02 ACC_COUNT,
 *   0x07 VBUS1, 0x08 VBUS2, 0x0B VSENSE1, 0x0C VSENSE2,
 *   0xFD PRODUCT_ID, 0xFE MANUFACTURER_ID, 0xFF REVISION_ID
 *
 * VBUS:   16-bit unsigned, FSR = 32V,     LSB = 32/65536 V
 * VSENSE: 16-bit unsigned, FSR = 100mV,   LSB = 100/65536 mV (POR default)
 */

#include "pac1952.h"
#include "contract.h"
#include <string.h>

/* ---- register addresses ---- */
#define REG_REFRESH         0x00u
#define REG_CTRL            0x01u
#define REG_ACC_COUNT       0x02u
#define REG_VBUS1           0x07u
#define REG_VBUS2           0x08u
#define REG_VSENSE1         0x0Bu
#define REG_VSENSE2         0x0Cu
#define REG_PRODUCT_ID      0xFDu
#define REG_MANUFACTURER_ID 0xFEu
#define REG_REVISION_ID     0xFFu

/* ---- conversion constants ---- */
#define VBUS_LSB    (32.0f / 65536.0f)      /* V per LSB   */
#define VSENSE_LSB  (100.0f / 65536.0f)     /* mV per LSB, unipolar POR default */

/* ---- timing ---- */
#define REFRESH_WAIT_MS 1u      /* results settle, commands get NACKed */

/* ---- CTRL fields ---- */
#define CTRL_HI_MODE_SHIFT  4u      /* SAMPLE_MODE is bits [15:12] */
#define CTRL_HI_OTHER_MASK  0x0Fu   /* GPIO_ALERT2 and SLOW_ALERT1 */

/* SAMPLE_MODE by pac1952_rate_t: the plain modes, not the adaptive ones */
static const uint8_t sample_mode[] = {
    [PAC1952_RATE_8SPS]    = 0x7u,
    [PAC1952_RATE_64SPS]   = 0x6u,
    [PAC1952_RATE_256SPS]  = 0x5u,
    [PAC1952_RATE_1024SPS] = 0x4u,
};

/* ---- internal helpers ---- */

/* REFRESH is a Send Byte: the command and no data.  The registers settle
 * within 1 ms of it, and the chip NACKs any command sent before that. */
static pac1952_err_t send_refresh(const pac1952_t *dev)
{
    const uint8_t cmd = REG_REFRESH;
    if (!dev->i2c->write_raw(dev->i2c->ctx, PAC1952_DEV_ADDR, &cmd, 1))
        return PAC1952_ERR_I2C;

    dev->i2c->delay_ms(dev->i2c->ctx, REFRESH_WAIT_MS);
    return PAC1952_OK;
}

static pac1952_err_t read_reg16(const pac1952_t *dev, uint8_t reg,
                                uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, PAC1952_DEV_ADDR, reg, buf, 2))
        return PAC1952_ERR_I2C;
    *val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return PAC1952_OK;
}

static pac1952_err_t read_channel_raw(const pac1952_t *dev, uint8_t ch,
                                      uint16_t *vbus_raw,
                                      uint16_t *vsense_raw)
{
    uint8_t vbus_reg   = (ch == 0) ? REG_VBUS1   : REG_VBUS2;
    uint8_t vsense_reg = (ch == 0) ? REG_VSENSE1 : REG_VSENSE2;

    uint16_t vb;
    pac1952_err_t err = read_reg16(dev, vbus_reg, &vb);
    if (err != PAC1952_OK)
        return err;

    uint16_t vs;
    err = read_reg16(dev, vsense_reg, &vs);
    if (err != PAC1952_OK)
        return err;

    *vbus_raw   = vb;
    *vsense_raw = vs;
    return PAC1952_OK;
}

static void convert_channel(const pac1952_t *dev, uint8_t ch,
                            uint16_t vbus_raw, uint16_t vsense_raw,
                            pac1952_channel_t *out)
{
    out->vbus_v    = (float)vbus_raw * VBUS_LSB;
    out->vsense_mv = (float)vsense_raw * VSENSE_LSB;

    float r_mohm = dev->cfg.sense_resistor_mohm[ch];
    if (r_mohm > 0.0f) {
        /* I = Vsense / Rsense.  Vsense in mV, Rsense in mOhm -> mA */
        out->current_ma = out->vsense_mv / r_mohm * 1000.0f;
        out->power_mw   = out->vbus_v * out->current_ma;
    } else {
        out->current_ma = 0.0f;
        out->power_mw   = 0.0f;
    }
}

/* ---- public API ---- */

pac1952_err_t pac1952_init(pac1952_t *dev, const pac1952_i2c_t *i2c,
                           const pac1952_cfg_t *cfg)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    REQUIRE(cfg != NULL);
    if (!dev)
        return PAC1952_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return PAC1952_ERR_PARAM;
    REQUIRE(i2c->read_regs != NULL);
    if (!i2c->read_regs)
        return PAC1952_ERR_PARAM;

    /* Validate sense resistors: must be non-negative */
    if (!(cfg->sense_resistor_mohm[0] >= 0.0f) ||
        !(cfg->sense_resistor_mohm[1] >= 0.0f))
        return PAC1952_ERR_PARAM;

    /* Read and verify PRODUCT_ID */
    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, PAC1952_DEV_ADDR, REG_PRODUCT_ID, &id, 1))
        return PAC1952_ERR_I2C;

    if (id != PAC1952_PRODUCT_ID_VAL && id != PAC1952_PRODUCT_ID_VAL_2)
        return PAC1952_ERR_ID;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    return PAC1952_OK;
}

pac1952_err_t pac1952_refresh(pac1952_t *dev)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->i2c)
        return PAC1952_ERR_PARAM;
    if (!dev->i2c->write_raw || !dev->i2c->delay_ms)
        return PAC1952_ERR_PARAM;

    return send_refresh(dev);
}

pac1952_err_t pac1952_read_channel(pac1952_t *dev, uint8_t ch,
                                   pac1952_channel_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    REQUIRE(ch <= 1);
    if (!dev || !dev->i2c || !out)
        return PAC1952_ERR_PARAM;
    if (ch > 1)
        return PAC1952_ERR_PARAM;

    uint16_t vbus_raw;
    uint16_t vsense_raw;
    pac1952_err_t err = read_channel_raw(dev, ch, &vbus_raw, &vsense_raw);
    if (err != PAC1952_OK)
        return err;

    convert_channel(dev, ch, vbus_raw, vsense_raw, out);
    return PAC1952_OK;
}

pac1952_err_t pac1952_read_both(pac1952_t *dev,
                                pac1952_channel_t *ch1,
                                pac1952_channel_t *ch2)
{
    REQUIRE(dev != NULL);
    REQUIRE(ch1 != NULL);
    REQUIRE(ch2 != NULL);
    if (!dev || !dev->i2c || !ch1 || !ch2)
        return PAC1952_ERR_PARAM;

    uint16_t vbus_raw;
    uint16_t vsense_raw;

    /* Channel 0 */
    pac1952_err_t err = read_channel_raw(dev, 0, &vbus_raw, &vsense_raw);
    if (err != PAC1952_OK)
        return err;
    convert_channel(dev, 0, vbus_raw, vsense_raw, ch1);

    /* Channel 1 */
    err = read_channel_raw(dev, 1, &vbus_raw, &vsense_raw);
    if (err != PAC1952_OK)
        return err;
    convert_channel(dev, 1, vbus_raw, vsense_raw, ch2);

    return PAC1952_OK;
}

pac1952_err_t pac1952_set_sample_rate(pac1952_t *dev, pac1952_rate_t rate)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->i2c)
        return PAC1952_ERR_PARAM;
    if ((unsigned)rate > 3u)
        return PAC1952_ERR_PARAM;
    if (!dev->i2c->write_raw || !dev->i2c->delay_ms)
        return PAC1952_ERR_PARAM;

    /* CTRL is 16 bits, so it takes a Block Write: the register, then high
     * and low byte.  Read it first to keep the pin and channel settings. */
    uint8_t frame[3];
    frame[0] = REG_CTRL;
    if (!dev->i2c->read_regs(dev->i2c->ctx, PAC1952_DEV_ADDR, REG_CTRL,
                             &frame[1], 2))
        return PAC1952_ERR_I2C;

    frame[1] = (uint8_t)((frame[1] & CTRL_HI_OTHER_MASK) |
                         (uint8_t)(sample_mode[rate] << CTRL_HI_MODE_SHIFT));
    if (!dev->i2c->write_raw(dev->i2c->ctx, PAC1952_DEV_ADDR, frame, 3))
        return PAC1952_ERR_I2C;

    /* A change to CTRL is activated by a REFRESH */
    return send_refresh(dev);
}

pac1952_err_t pac1952_get_product_id(pac1952_t *dev, uint8_t *id)
{
    REQUIRE(dev != NULL);
    REQUIRE(id != NULL);
    if (!dev || !dev->i2c || !id)
        return PAC1952_ERR_PARAM;

    if (!dev->i2c->read_regs(dev->i2c->ctx, PAC1952_DEV_ADDR,
                             REG_PRODUCT_ID, id, 1))
        return PAC1952_ERR_I2C;

    return PAC1952_OK;
}
