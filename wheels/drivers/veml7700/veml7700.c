// SPDX-License-Identifier: MIT
/*
 * veml7700 — Vishay VEML7700 I2C ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (all 16-bit little-endian):
 *   0x00  ALS_CONF — gain[12:11], IT[9:6], SD[0]
 *   0x04  ALS data
 *   0x05  White data
 *
 * Lux = raw_als x resolution, where resolution depends on gain x IT.
 * Base resolution: gain=1, IT=100ms -> 0.0672 lux/bit (Vishay 84286 rev 1.8).
 */

#include "veml7700.h"
#include "contract.h"

/* --- register addresses --- */
#define REG_ALS_CONF   0x00u
#define REG_ALS_DATA   0x04u
#define REG_WHITE_DATA 0x05u

/* --- ALS_CONF bit positions --- */
#define CONF_SD_BIT      0u
#define CONF_IT_SHIFT    6u
#define CONF_IT_MASK     0x0Fu
#define CONF_GAIN_SHIFT  11u
#define CONF_GAIN_MASK   0x03u

/* --- base resolution at gain=1, IT=100ms (lux per count) --- */
#define BASE_RESOLUTION  0.0672f

/* ---- helpers ---- */

static veml7700_err_t write_reg16(const veml7700_t *dev, uint8_t reg,
                                  uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(val & 0xFFu);         /* low byte first */
    buf[1] = (uint8_t)((val >> 8u) & 0xFFu);
    if (!dev->i2c->write_regs(dev->i2c->ctx, VEML7700_DEV_ADDR,
                              reg, buf, 2))
        return VEML7700_ERR_I2C;
    return VEML7700_OK;
}

static veml7700_err_t read_reg16(const veml7700_t *dev, uint8_t reg,
                                 uint16_t *val)
{
    uint8_t buf[2];
    if (!dev->i2c->read_regs(dev->i2c->ctx, VEML7700_DEV_ADDR,
                             reg, buf, 2))
        return VEML7700_ERR_I2C;
    *val = (uint16_t)(((uint16_t)buf[1] << 8u) | (uint16_t)buf[0]);
    return VEML7700_OK;
}

static uint16_t build_conf(veml7700_gain_t gain, veml7700_it_t it,
                            bool shutdown)
{
    uint16_t conf = 0;
    conf = (uint16_t)(conf | ((uint16_t)(gain & CONF_GAIN_MASK) << CONF_GAIN_SHIFT));
    conf = (uint16_t)(conf | ((uint16_t)(it & CONF_IT_MASK) << CONF_IT_SHIFT));
    if (shutdown) conf = (uint16_t)(conf | (1u << CONF_SD_BIT));
    return conf;
}

static float gain_factor(veml7700_gain_t gain)
{
    switch (gain) {
    case VEML7700_GAIN_2:   return 2.0f;
    case VEML7700_GAIN_1:   return 1.0f;
    case VEML7700_GAIN_1_4: return 0.25f;
    case VEML7700_GAIN_1_8: return 0.125f;
    default:                return 1.0f;
    }
}

static float it_factor(veml7700_it_t it)
{
    switch (it) {
    case VEML7700_IT_800MS: return 8.0f;
    case VEML7700_IT_400MS: return 4.0f;
    case VEML7700_IT_200MS: return 2.0f;
    case VEML7700_IT_100MS: return 1.0f;
    case VEML7700_IT_50MS:  return 0.5f;
    case VEML7700_IT_25MS:  return 0.25f;
    default:                return 1.0f;
    }
}

static float resolution(veml7700_gain_t gain, veml7700_it_t it)
{
    float gf = gain_factor(gain);
    float itf = it_factor(it);
    /* Higher gain/IT -> smaller resolution (more sensitive) */
    return BASE_RESOLUTION / (gf * itf);
}

static veml7700_err_t write_conf(veml7700_t *dev, bool shutdown)
{
    uint16_t conf = build_conf(dev->gain, dev->it, shutdown);
    return write_reg16(dev, REG_ALS_CONF, conf);
}

/* ---- public API ---- */

static bool valid_gain(veml7700_gain_t g)
{
    return g == VEML7700_GAIN_1 || g == VEML7700_GAIN_2 ||
           g == VEML7700_GAIN_1_8 || g == VEML7700_GAIN_1_4;
}

static bool valid_it(veml7700_it_t it)
{
    return it == VEML7700_IT_25MS  || it == VEML7700_IT_50MS  ||
           it == VEML7700_IT_100MS || it == VEML7700_IT_200MS ||
           it == VEML7700_IT_400MS || it == VEML7700_IT_800MS;
}

veml7700_err_t veml7700_init(veml7700_t *dev, const veml7700_i2c_t *i2c,
                             veml7700_gain_t gain, veml7700_it_t it)
{
    if (!dev || !i2c || !i2c->write_regs || !i2c->read_regs)
        return VEML7700_ERR_PARAM;
    if (!valid_gain(gain) || !valid_it(it))
        return VEML7700_ERR_PARAM;

    dev->i2c  = i2c;
    dev->gain = gain;
    dev->it   = it;

    return write_conf(dev, false);
}

veml7700_err_t veml7700_read_als(veml7700_t *dev, uint16_t *raw)
{
    if (!dev || !raw) return VEML7700_ERR_PARAM;
    return read_reg16(dev, REG_ALS_DATA, raw);
}

veml7700_err_t veml7700_read_white(veml7700_t *dev, uint16_t *raw)
{
    if (!dev || !raw) return VEML7700_ERR_PARAM;
    return read_reg16(dev, REG_WHITE_DATA, raw);
}

veml7700_err_t veml7700_read_lux(veml7700_t *dev, float *lux)
{
    if (!dev || !lux) return VEML7700_ERR_PARAM;

    uint16_t raw;
    veml7700_err_t err = read_reg16(dev, REG_ALS_DATA, &raw);
    if (err != VEML7700_OK) return err;

    *lux = (float)raw * resolution(dev->gain, dev->it);
    ENSURE(*lux == *lux);  /* not NaN */
    ENSURE(*lux >= 0.0f);
    return VEML7700_OK;
}

veml7700_err_t veml7700_set_gain(veml7700_t *dev, veml7700_gain_t gain)
{
    if (!dev) return VEML7700_ERR_PARAM;
    dev->gain = gain;
    return write_conf(dev, false);
}

veml7700_err_t veml7700_set_integration_time(veml7700_t *dev,
                                              veml7700_it_t it)
{
    if (!dev) return VEML7700_ERR_PARAM;
    dev->it = it;
    return write_conf(dev, false);
}

veml7700_err_t veml7700_power_on(veml7700_t *dev)
{
    if (!dev) return VEML7700_ERR_PARAM;
    return write_conf(dev, false);
}

veml7700_err_t veml7700_shutdown(veml7700_t *dev)
{
    if (!dev) return VEML7700_ERR_PARAM;
    return write_conf(dev, true);
}
