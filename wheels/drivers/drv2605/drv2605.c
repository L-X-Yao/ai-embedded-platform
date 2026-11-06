// SPDX-License-Identifier: MIT
/*
 * drv2605 -- TI DRV2605L I2C haptic motor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map from TI DRV2605L datasheet (SLOS854A).
 */

#include "drv2605.h"
#include "contract.h"

/* ---------- register addresses ---------- */

#define REG_STATUS      0x00u
#define REG_MODE        0x01u
#define REG_RTP         0x02u
#define REG_LIBRARY     0x03u
#define REG_WAVEFORM0   0x04u  /* 0x04 .. 0x0B = 8 slots */
#define REG_GO          0x0Cu
#define REG_FEEDBACK    0x1Au
#define REG_CONTROL3    0x1Du

#define STATUS_DEVID_SHIFT  5u
#define STATUS_DEVID_MASK   0xE0u

#define MODE_STANDBY        0x40u

#define FEEDBACK_LRA_BIT    0x80u

#define LIBRARY_SEL_MASK    0x07u
#define LIBRARY_ERM_A       0x01u
#define LIBRARY_LRA         0x06u

#define GO_BIT              0x01u

/* ---------- helpers ---------- */

static bool write_reg(const drv2605_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, DRV2605_ADDR, reg, val);
}

static bool read_reg(const drv2605_t *dev, uint8_t reg, uint8_t *val)
{
    return dev->i2c->read_regs(dev->i2c->ctx, DRV2605_ADDR, reg, val, 1);
}

/* The ROM effects come from a library matched to the actuator */
static drv2605_err_t select_library(const drv2605_t *dev,
                                    drv2605_actuator_t type)
{
    uint8_t lib = 0;
    if (!read_reg(dev, REG_LIBRARY, &lib))
        return DRV2605_ERR_I2C;

    lib &= (uint8_t)~LIBRARY_SEL_MASK;
    lib |= (type == DRV2605_ACTUATOR_LRA) ? LIBRARY_LRA : LIBRARY_ERM_A;

    if (!write_reg(dev, REG_LIBRARY, lib))
        return DRV2605_ERR_I2C;

    return DRV2605_OK;
}

/* ---------- API ---------- */

drv2605_err_t drv2605_init(drv2605_t *dev, const drv2605_i2c_t *i2c)
{
    if (!dev || !i2c || !i2c->write_reg || !i2c->read_regs)
        return DRV2605_ERR_PARAM;

    dev->i2c = i2c;

    /* Read status register and check device ID (bits [7:5]) */
    uint8_t status = 0;
    if (!read_reg(dev, REG_STATUS, &status))
        return DRV2605_ERR_I2C;

    uint8_t devid = (status & STATUS_DEVID_MASK) >> STATUS_DEVID_SHIFT;
    if (devid != DRV2605_DEVID_2605 && devid != DRV2605_DEVID_2605L)
        return DRV2605_ERR_ID;

    /* Exit standby: set mode to internal trigger */
    if (!write_reg(dev, REG_MODE, DRV2605_MODE_INTERNAL_TRIGGER))
        return DRV2605_ERR_I2C;

    /* Clear all waveform slots */
    for (uint8_t i = 0; i < DRV2605_WAVEFORM_SLOTS; i++) {
        if (!write_reg(dev, (uint8_t)(REG_WAVEFORM0 + i), 0))
            return DRV2605_ERR_I2C;
    }

    /* Initialization Procedure step 6 (DRV2605 sheet p.53, DRV2605L p.59).
     * The DRV2605 sheet gives 0x03 a default of 0x00, the Empty library, on
     * p.33 and of 1 on p.36, so the default is not relied on. */
    uint8_t fb = 0;
    if (!read_reg(dev, REG_FEEDBACK, &fb))
        return DRV2605_ERR_I2C;

    drv2605_err_t err = select_library(dev, (fb & FEEDBACK_LRA_BIT) != 0u
                                            ? DRV2605_ACTUATOR_LRA
                                            : DRV2605_ACTUATOR_ERM);
    if (err != DRV2605_OK)
        return err;

    INVARIANT(dev->i2c != NULL);
    return DRV2605_OK;
}

drv2605_err_t drv2605_set_actuator(drv2605_t *dev, drv2605_actuator_t type)
{
    if (!dev || !dev->i2c)
        return DRV2605_ERR_PARAM;
    if ((unsigned)type > 1)
        return DRV2605_ERR_PARAM;

    /* Read-modify-write the feedback control register */
    uint8_t fb = 0;
    if (!read_reg(dev, REG_FEEDBACK, &fb))
        return DRV2605_ERR_I2C;

    if (type == DRV2605_ACTUATOR_LRA)
        fb |= FEEDBACK_LRA_BIT;
    else
        fb &= (uint8_t)~FEEDBACK_LRA_BIT;

    if (!write_reg(dev, REG_FEEDBACK, fb))
        return DRV2605_ERR_I2C;

    return select_library(dev, type);
}

drv2605_err_t drv2605_set_waveform(drv2605_t *dev, uint8_t slot,
                                   uint8_t effect_id)
{
    if (!dev || !dev->i2c)
        return DRV2605_ERR_PARAM;
    if (slot >= DRV2605_WAVEFORM_SLOTS)
        return DRV2605_ERR_PARAM;

    if (!write_reg(dev, (uint8_t)(REG_WAVEFORM0 + slot), effect_id))
        return DRV2605_ERR_I2C;

    return DRV2605_OK;
}

drv2605_err_t drv2605_go(drv2605_t *dev)
{
    if (!dev || !dev->i2c)
        return DRV2605_ERR_PARAM;

    /* GO plays the sequencer only in internal trigger mode */
    if (!write_reg(dev, REG_MODE, DRV2605_MODE_INTERNAL_TRIGGER))
        return DRV2605_ERR_I2C;

    if (!write_reg(dev, REG_GO, GO_BIT))
        return DRV2605_ERR_I2C;

    return DRV2605_OK;
}

drv2605_err_t drv2605_stop(drv2605_t *dev)
{
    if (!dev || !dev->i2c)
        return DRV2605_ERR_PARAM;

    if (!write_reg(dev, REG_GO, 0x00))
        return DRV2605_ERR_I2C;

    /* RTP mode keeps driving the actuator until MODE changes */
    if (!write_reg(dev, REG_MODE, DRV2605_MODE_INTERNAL_TRIGGER))
        return DRV2605_ERR_I2C;

    return DRV2605_OK;
}

drv2605_err_t drv2605_set_realtime_value(drv2605_t *dev, uint8_t value)
{
    if (!dev || !dev->i2c)
        return DRV2605_ERR_PARAM;

    if (!write_reg(dev, REG_RTP, value))
        return DRV2605_ERR_I2C;

    /* The chip drives RTP_INPUT only in RTP mode */
    if (!write_reg(dev, REG_MODE, DRV2605_MODE_REALTIME))
        return DRV2605_ERR_I2C;

    return DRV2605_OK;
}
