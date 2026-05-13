// SPDX-License-Identifier: MIT
/*
 * pct2075 — NXP PCT2075 digital temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * PCT2075 register map (datasheet):
 *   0x00  Temperature (read-only, 2 bytes, 11-bit two's complement in [15:5])
 *   0x01  Configuration (R/W, 1 byte)
 *   0x02  T_hyst (R/W, 2 bytes, same encoding as temperature)
 *   0x03  T_os   (R/W, 2 bytes, same encoding as temperature)
 *   0x04  T_idle (R/W, 1 byte)
 */

#include "pct2075.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_TEMP    0x00u
#define REG_CONF    0x01u

/* ---------- internal helpers ---------- */

static bool read_regs(const pct2075_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, len);
}

/* Convert raw 16-bit register to temperature in degrees C.
 * The 11-bit value sits in bits [15:5], sign-extended.
 * Shift right 5, then multiply by 0.125 C/LSB. */
static float raw_to_temp(int16_t raw)
{
    int16_t shifted = (int16_t)(raw >> 5);  /* arithmetic right shift */
    return (float)shifted * 0.125f;
}

/* ---------- public API ---------- */

pct2075_err_t pct2075_init(pct2075_t *dev, const pct2075_i2c_t *i2c,
                            const pct2075_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg) {
        return PCT2075_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg) {
        return PCT2075_ERR_PARAM;
    }
    /* Valid PCT2075 addresses: 0x48 through 0x4F (A0-A2 pins) */
    if (cfg->addr < 0x48u || cfg->addr > 0x4Fu) {
        return PCT2075_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c  = i2c;
    dev->addr = cfg->addr;

    /* Verify communication by reading the temperature register.
     * PCT2075 has no chip ID register, so we just confirm the bus works. */
    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP, buf, 2)) {
        return PCT2075_ERR_I2C;
    }

    /* Optionally write configuration register */
    if (cfg->write_config) {
        if (!i2c->write_reg(i2c->ctx, dev->addr, REG_CONF, cfg->conf_val)) {
            return PCT2075_ERR_I2C;
        }
    }

    INVARIANT(dev->i2c != NULL);
    return PCT2075_OK;
}

pct2075_err_t pct2075_read(pct2075_t *dev, float *temp_c)
{
    if (!dev || !temp_c) {
        return PCT2075_ERR_PARAM;
    }
    if (!dev->i2c) {
        return PCT2075_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP, buf, 2)) {
        return PCT2075_ERR_I2C;
    }

    int16_t raw = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    *temp_c = raw_to_temp(raw);

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    ENSURE(*temp_c >= -55.0f && *temp_c <= 125.0f);
    return PCT2075_OK;
}
