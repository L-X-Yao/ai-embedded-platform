// SPDX-License-Identifier: MIT
/*
 * ccs811 — AMS/ScioSense CCS811 digital eCO2/TVOC gas sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map from CCS811 datasheet (ams AG, AS000000_CCS811).
 */

#include "ccs811.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses (from datasheet Table 12) ---------- */

#define REG_STATUS       0x00u
#define REG_MEAS_MODE    0x01u
#define REG_ALG_RESULT   0x02u  /* 8 bytes: eCO2(2), TVOC(2), status, error, raw(2) */
#define REG_ENV_DATA     0x05u
#define REG_HW_ID        0x20u
#define REG_APP_START    0xF4u

/* STATUS register bits */
#define STATUS_APP_VALID  0x10u
#define STATUS_DATA_READY 0x08u
#define STATUS_FW_MODE    0x80u

static bool write_reg(const ccs811_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const ccs811_t *dev, uint8_t reg, uint8_t *buf,
                      uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static void delay(const ccs811_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms) {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return CCS811_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return CCS811_ERR_PARAM;
    if ((unsigned)cfg->mode > 3)
        return CCS811_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Read and verify hardware ID */
    uint8_t hw_id = 0;
    if (!read_regs(dev, REG_HW_ID, &hw_id, 1))
        return CCS811_ERR_I2C;
    if (hw_id != CCS811_HW_ID)
        return CCS811_ERR_ID;

    /* Check STATUS: APP_VALID must be set */
    uint8_t status = 0;
    if (!read_regs(dev, REG_STATUS, &status, 1))
        return CCS811_ERR_I2C;
    if (!(status & STATUS_APP_VALID))
        return CCS811_ERR_APP;

    /* Start application mode by writing to APP_START (no data byte needed,
     * but our HAL requires a val — device ignores it) */
    if (!write_reg(dev, REG_APP_START, 0))
        return CCS811_ERR_I2C;
    delay(dev, 1);

    /* Set measurement mode */
    uint8_t meas_mode = (uint8_t)((cfg->mode & 0x07u) << 4);
    if (!write_reg(dev, REG_MEAS_MODE, meas_mode))
        return CCS811_ERR_I2C;

    INVARIANT(dev->i2c != NULL);
    return CCS811_OK;
}

ccs811_err_t ccs811_read(ccs811_t *dev, ccs811_data_t *out)
{
    if (!dev || !out)
        return CCS811_ERR_PARAM;

    /* Check data ready */
    uint8_t status = 0;
    if (!read_regs(dev, REG_STATUS, &status, 1))
        return CCS811_ERR_I2C;
    if (!(status & STATUS_DATA_READY))
        return CCS811_ERR_NO_DATA;

    /* Read algorithm result (4 bytes: eCO2 MSB, LSB, TVOC MSB, LSB) */
    uint8_t buf[4];
    if (!read_regs(dev, REG_ALG_RESULT, buf, 4))
        return CCS811_ERR_I2C;

    out->eco2_ppm = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    out->tvoc_ppb = (uint16_t)((uint16_t)buf[2] << 8 | buf[3]);

    return CCS811_OK;
}

ccs811_err_t ccs811_set_env_data(ccs811_t *dev, float humidity_pct,
                                 float temperature_c)
{
    if (!dev)
        return CCS811_ERR_PARAM;

    /* Encode humidity: fixed-point 7.1 format (value * 512) */
    uint16_t hum_enc;
    if (humidity_pct < 0.0f) {
        hum_enc = 0;
    } else if (humidity_pct > 100.0f) {
        hum_enc = (uint16_t)(100u << 9);
    } else {
        hum_enc = (uint16_t)(humidity_pct * 512.0f);
    }

    /* Encode temperature: offset by +25C, fixed-point 7.1 format */
    float temp_offset = temperature_c + 25.0f;
    uint16_t temp_enc;
    if (temp_offset < 0.0f) {
        temp_enc = 0;
    } else {
        temp_enc = (uint16_t)(temp_offset * 512.0f);
    }

    /* Write ENV_DATA as 4 sequential register bytes.
     * CCS811 expects: hum_hi, hum_lo, temp_hi, temp_lo at reg 0x05..0x08 */
    if (!write_reg(dev, REG_ENV_DATA, (uint8_t)(hum_enc >> 8)))
        return CCS811_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(REG_ENV_DATA + 1u), (uint8_t)(hum_enc & 0xFF)))
        return CCS811_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(REG_ENV_DATA + 2u), (uint8_t)(temp_enc >> 8)))
        return CCS811_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(REG_ENV_DATA + 3u), (uint8_t)(temp_enc & 0xFF)))
        return CCS811_ERR_I2C;

    return CCS811_OK;
}
