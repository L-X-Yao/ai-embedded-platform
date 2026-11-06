// SPDX-License-Identifier: MIT
/*
 * ccs811 — AMS CCS811 eCO2/TVOC gas sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Boot and measurement protocol from CCS811 datasheet (AMS).
 */

#include "ccs811.h"
#include "contract.h"
#include <string.h>

/* Registers */
#define REG_STATUS       0x00
#define REG_MEAS_MODE    0x01
#define REG_ALG_RESULT   0x02
#define REG_ENV_DATA     0x05
#define REG_HW_ID        0x20
#define REG_APP_START    0xF4

/* STATUS bits */
#define STATUS_APP_VALID   (1u << 4)
#define STATUS_FW_MODE     (1u << 7)
#define STATUS_DATA_READY  (1u << 3)

static bool write_reg(const ccs811_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const ccs811_t *dev, uint8_t reg, uint8_t *buf,
                      uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static bool write_raw(const ccs811_t *dev, const uint8_t *data, uint16_t len)
{
    return dev->i2c->write_raw(dev->i2c->ctx, dev->cfg.addr, data, len);
}

static void delay(const ccs811_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return CCS811_ERR_PARAM;
    }

    if (!i2c->write_raw)
    {
        return CCS811_ERR_PARAM;
    }

    if ((unsigned)cfg->mode > 4)
    {
        return CCS811_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Check hardware ID */
    uint8_t hw_id = 0;
    if (!read_regs(dev, REG_HW_ID, &hw_id, 1))
    {
        return CCS811_ERR_I2C;
    }
    if (hw_id != CCS811_HW_ID)
    {
        return CCS811_ERR_ID;
    }

    /* Check STATUS: APP_VALID must be set */
    uint8_t status = 0;
    if (!read_regs(dev, REG_STATUS, &status, 1))
    {
        return CCS811_ERR_I2C;
    }
    if (!(status & STATUS_APP_VALID))
    {
        return CCS811_ERR_BOOT;
    }

    /* APP_START (0xF4) goes out alone, with no data, to leave boot mode */
    const uint8_t app_start = REG_APP_START;
    if (!write_raw(dev, &app_start, 1))
    {
        return CCS811_ERR_I2C;
    }
    delay(dev, 1);

    /* Verify FW_MODE is set */
    if (!read_regs(dev, REG_STATUS, &status, 1))
    {
        return CCS811_ERR_I2C;
    }
    if (!(status & STATUS_FW_MODE))
    {
        return CCS811_ERR_BOOT;
    }

    /* Set drive mode */
    uint8_t meas_mode = (uint8_t)((cfg->mode & 0x07) << 4);
    if (!write_reg(dev, REG_MEAS_MODE, meas_mode))
    {
        return CCS811_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return CCS811_OK;
}

ccs811_err_t ccs811_read(const ccs811_t *dev, ccs811_data_t *out)
{
    if (!dev || !out)
    {
        return CCS811_ERR_PARAM;
    }

    if (dev->cfg.mode == CCS811_MODE_250MS)
    {
        return CCS811_ERR_MODE;
    }

    /* Check data ready */
    uint8_t status = 0;
    if (!read_regs(dev, REG_STATUS, &status, 1))
    {
        return CCS811_ERR_I2C;
    }
    if (!(status & STATUS_DATA_READY))
    {
        return CCS811_ERR_NOT_READY;
    }

    /* Read 4 bytes from ALG_RESULT_DATA */
    uint8_t buf[4];
    if (!read_regs(dev, REG_ALG_RESULT, buf, 4))
    {
        return CCS811_ERR_I2C;
    }

    out->eco2_ppm = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    out->tvoc_ppb = (uint16_t)((uint16_t)buf[2] << 8 | buf[3]);

    return CCS811_OK;
}

ccs811_err_t ccs811_set_mode(ccs811_t *dev, ccs811_drive_mode_t mode)
{
    if (!dev)
    {
        return CCS811_ERR_PARAM;
    }
    if ((unsigned)mode > 4)
    {
        return CCS811_ERR_PARAM;
    }

    uint8_t meas_mode = (uint8_t)((mode & 0x07) << 4);
    if (!write_reg(dev, REG_MEAS_MODE, meas_mode))
    {
        return CCS811_ERR_I2C;
    }

    dev->cfg.mode = mode;
    return CCS811_OK;
}

ccs811_err_t ccs811_set_env_data(const ccs811_t *dev, uint16_t humidity,
                                 uint16_t temperature)
{
    if (!dev)
    {
        return CCS811_ERR_PARAM;
    }

    /* A 0.5 step is one count of the high byte, so larger values do not fit */
    if (humidity > 0xFF || temperature > 0xFF)
    {
        return CCS811_ERR_PARAM;
    }

    /*
     * ENV_DATA (0x05) is one 4-byte mailbox, written in one transaction:
     * humidity high and low, then temperature high and low, each in 1/512
     * units. The low bytes hold the fraction below 0.5, which is always 0.
     */
    const uint8_t frame[5] = {
        REG_ENV_DATA,
        (uint8_t)humidity, 0x00,
        (uint8_t)temperature, 0x00,
    };
    if (!write_raw(dev, frame, sizeof(frame)))
    {
        return CCS811_ERR_I2C;
    }

    return CCS811_OK;
}
