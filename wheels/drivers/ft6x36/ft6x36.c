// SPDX-License-Identifier: MIT
/*
 * ft6x36 — FocalTech FT6236/FT6336 capacitive touch controller (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register layout reference: FocalTech FT6236 datasheet.
 * Touch point data: 6 bytes per point starting at P1_XH (0x03).
 *   Byte 0 (PnXH): [event(7:6)][reserved(5:4)][X_high(3:0)]
 *   Byte 1 (PnXL): [X_low(7:0)]
 *   Byte 2 (PnYH): [touch_id(7:4)][Y_high(3:0)]
 *   Byte 3 (PnYL): [Y_low(7:0)]
 */

#include "ft6x36.h"
#include "contract.h"
#include <string.h>

/* Touch point data offsets within a 6-byte block */
#define TP_XH_OFFSET  0u
#define TP_XL_OFFSET  1u
#define TP_YH_OFFSET  2u
#define TP_YL_OFFSET  3u

/* Masks for touch point register fields */
#define TP_EVENT_MASK     0xC0u
#define TP_EVENT_SHIFT    6u
#define TP_COORD_HI_MASK  0x0Fu

/* Max touch count the chip reports */
#define FT6X36_MAX_TOUCH  2u

static ft6x36_err_t read_reg8(const ft6x36_t *dev, uint8_t reg, uint8_t *val)
{
    if (!dev->i2c->read_regs(dev->i2c->ctx, FT6X36_DEV_ADDR, reg, val, 1))
        return FT6X36_ERR_I2C;
    return FT6X36_OK;
}

ft6x36_err_t ft6x36_init(ft6x36_t *dev, const ft6x36_i2c_t *i2c)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    if (!dev) return FT6X36_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it, whose chip was never verified */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !i2c->write_reg || !i2c->read_regs) return FT6X36_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    ft6x36_err_t err = read_reg8(dev, FT6X36_REG_CHIP_ID, &id);
    if (err != FT6X36_OK) {
        dev->i2c = NULL;
        return err;
    }

    if (id != FT6X36_CHIP_ID_6236 && id != FT6X36_CHIP_ID_6336) {
        dev->i2c = NULL;
        return FT6X36_ERR_ID;
    }

    dev->chip_id = id;
    return FT6X36_OK;
}

ft6x36_err_t ft6x36_read_touch(ft6x36_t *dev, ft6x36_touch_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    if (!dev || !dev->i2c || !out) return FT6X36_ERR_PARAM;

    /* Read TD_STATUS (0x02) through P2_YL (0x0C) = 11 bytes */
    uint8_t buf[11];
    if (!dev->i2c->read_regs(dev->i2c->ctx, FT6X36_DEV_ADDR,
                             FT6X36_REG_TD_STATUS, buf, 11))
        return FT6X36_ERR_I2C;

    uint8_t count = buf[0] & 0x0Fu;
    if (count > FT6X36_MAX_TOUCH) count = FT6X36_MAX_TOUCH;
    out->touch_count = count;

    /* Clear all fields first */
    out->x1 = 0; out->y1 = 0; out->event1 = FT6X36_EVT_PRESS;
    out->x2 = 0; out->y2 = 0; out->event2 = FT6X36_EVT_PRESS;

    if (count >= 1u) {
        /* Point 1: buf[1..4] = P1_XH..P1_YL (offset from TD_STATUS) */
        const uint8_t *p = &buf[1];
        out->event1 = (ft6x36_event_t)((p[TP_XH_OFFSET] & TP_EVENT_MASK)
                                       >> TP_EVENT_SHIFT);
        out->x1 = (uint16_t)(((uint16_t)(p[TP_XH_OFFSET] & TP_COORD_HI_MASK) << 8)
                             | (uint16_t)p[TP_XL_OFFSET]);
        out->y1 = (uint16_t)(((uint16_t)(p[TP_YH_OFFSET] & TP_COORD_HI_MASK) << 8)
                             | (uint16_t)p[TP_YL_OFFSET]);
    }

    if (count >= 2u) {
        /* Point 2: buf[7..10] = P2_XH..P2_YL (offset from TD_STATUS)
         * P2_XH is at register 0x09, TD_STATUS at 0x02, so offset = 7 */
        const uint8_t *p = &buf[7];
        out->event2 = (ft6x36_event_t)((p[TP_XH_OFFSET] & TP_EVENT_MASK)
                                       >> TP_EVENT_SHIFT);
        out->x2 = (uint16_t)(((uint16_t)(p[TP_XH_OFFSET] & TP_COORD_HI_MASK) << 8)
                             | (uint16_t)p[TP_XL_OFFSET]);
        out->y2 = (uint16_t)(((uint16_t)(p[TP_YH_OFFSET] & TP_COORD_HI_MASK) << 8)
                             | (uint16_t)p[TP_YL_OFFSET]);
    }

    return FT6X36_OK;
}

ft6x36_err_t ft6x36_set_threshold(ft6x36_t *dev, uint8_t value)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->i2c) return FT6X36_ERR_PARAM;

    if (!dev->i2c->write_reg(dev->i2c->ctx, FT6X36_DEV_ADDR,
                             FT6X36_REG_TH_GROUP, value))
        return FT6X36_ERR_I2C;

    return FT6X36_OK;
}

ft6x36_err_t ft6x36_get_chip_id(ft6x36_t *dev, uint8_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    if (!dev || !dev->i2c || !out) return FT6X36_ERR_PARAM;
    return read_reg8(dev, FT6X36_REG_CHIP_ID, out);
}

ft6x36_err_t ft6x36_get_firmware_version(ft6x36_t *dev, uint8_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    if (!dev || !dev->i2c || !out) return FT6X36_ERR_PARAM;
    return read_reg8(dev, FT6X36_REG_FW_VER, out);
}
