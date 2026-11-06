// SPDX-License-Identifier: MIT
/*
 * cst816s — Hynitron CST816S capacitive touch controller (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map from CST816S datasheet. Touch data read in a single
 * burst of 6 bytes (0x01..0x06) for efficiency.
 */

#include "cst816s.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_GESTURE_ID       0x01u
#define REG_FINGER_NUM       0x02u
#define REG_XH               0x03u
#define REG_XL               0x04u
#define REG_YH               0x05u
#define REG_YL               0x06u
#define REG_CHIP_ID          0xA7u
#define REG_FW_VERSION       0xA9u
#define REG_MOTION_MASK      0xECu
#define REG_AUTO_SLEEP_TIME  0xF9u
#define REG_DIS_AUTO_SLEEP   0xFEu

/* ---------- internal helpers ---------- */

static bool write_reg(const cst816s_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, CST816S_DEV_ADDR, reg, val);
}

static bool read_regs(const cst816s_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, CST816S_DEV_ADDR, reg, buf, len);
}

/* Neither Hynitron sheet gives a value for 0xA7. Zephyr takes 0xB4 and 0xB5 as
 * a CST816S, and ESPHome lists 0xB4, 0xB5 and 0xB6 for the S, T and D parts. */
static bool chip_id_known(uint8_t id)
{
    return id == 0xB4u || id == 0xB5u || id == 0xB6u;
}

/* ---------- public API ---------- */

cst816s_err_t cst816s_init(cst816s_t *dev,
                           const cst816s_i2c_t *i2c)
{
    if (!dev || !i2c) return CST816S_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return CST816S_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* Read and verify chip ID (register 0xA7) */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return CST816S_ERR_I2C;

    if (!chip_id_known(id))
        return CST816S_ERR_ID;

    dev->chip_id = id;

    /* Read firmware version */
    uint8_t fw = 0;
    if (!read_regs(dev, REG_FW_VERSION, &fw, 1))
        return CST816S_ERR_I2C;

    dev->fw_version = fw;

    return CST816S_OK;
}

cst816s_err_t cst816s_read(cst816s_t *dev,
                           cst816s_touch_t *out)
{
    if (!dev || !out) return CST816S_ERR_PARAM;

    /* Burst read registers 0x01..0x06 (6 bytes) */
    uint8_t buf[6];
    if (!read_regs(dev, REG_GESTURE_ID, buf, 6))
        return CST816S_ERR_I2C;

    out->gesture = (cst816s_gesture_t)buf[0];                  /* 0x01 */
    out->fingers = buf[1];                                      /* 0x02 */
    out->event   = (cst816s_event_t)((buf[2] >> 6) & 0x03u);  /* 0x03 bits 7:6 */
    out->x       = (uint16_t)(((uint16_t)(buf[2] & 0x0Fu) << 8) | buf[3]);
    out->y       = (uint16_t)(((uint16_t)(buf[4] & 0x0Fu) << 8) | buf[5]);

    return CST816S_OK;
}

cst816s_err_t cst816s_set_auto_sleep(cst816s_t *dev, bool enable)
{
    if (!dev) return CST816S_ERR_PARAM;

    /* 0xFE: 0 = auto-sleep enabled, non-zero = auto-sleep disabled */
    uint8_t val = enable ? (uint8_t)0x00u : (uint8_t)0x01u;
    if (!write_reg(dev, REG_DIS_AUTO_SLEEP, val))
        return CST816S_ERR_I2C;

    return CST816S_OK;
}

cst816s_err_t cst816s_set_motion_mask(cst816s_t *dev,
                                      bool continuous,
                                      bool double_click,
                                      bool long_press)
{
    if (!dev) return CST816S_ERR_PARAM;

    /* 0xEC: bit 2 = EnConLR, bit 1 = EnConUD, bit 0 = EnDClick */
    (void)long_press;
    uint8_t val = 0;
    if (continuous)   val |= 0x06u;
    if (double_click) val |= 0x01u;

    if (!write_reg(dev, REG_MOTION_MASK, val))
        return CST816S_ERR_I2C;

    return CST816S_OK;
}

uint8_t cst816s_get_chip_id(const cst816s_t *dev)
{
    if (!dev) return 0;
    return dev->chip_id;
}

uint8_t cst816s_get_firmware_version(const cst816s_t *dev)
{
    if (!dev) return 0;
    return dev->fw_version;
}
