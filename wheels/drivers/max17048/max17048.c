// SPDX-License-Identifier: MIT
/*
 * max17048 — MAX17048/MAX17049 1-cell LiPo fuel gauge (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "max17048.h"
#include "contract.h"

/* ─── register map ───────────────────────────────────────────── */

#define REG_VCELL    0x02u
#define REG_SOC      0x04u
#define REG_MODE     0x06u
#define REG_VERSION  0x08u
#define REG_CONFIG   0x0Cu
#define REG_CRATE    0x16u
#define REG_STATUS   0x1Au

#define MODE_EN_SLEEP  0x2000u

/* STATUS high byte: X EnVR SC HD VR VL VH RI */
#define STATUS_FLAGS   0x3F00u

/* ─── helpers ────────────────────────────────────────────────── */

static bool rd16(const max17048_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    if (!dev->i2c->read_regs(dev->i2c->ctx, MAX17048_DEV_ADDR, reg, buf, 2))
        return false;
    *val = (uint16_t)((buf[0] << 8) | buf[1]);
    return true;
}

static bool wr16(const max17048_t *dev, uint8_t reg, uint16_t val)
{
    if (!dev->i2c->write_reg(dev->i2c->ctx, MAX17048_DEV_ADDR, reg,
                             (uint8_t)(val >> 8)))
        return false;
    return dev->i2c->write_reg(dev->i2c->ctx, MAX17048_DEV_ADDR,
                               (uint8_t)(reg + 1), (uint8_t)(val & 0xFF));
}

/* ─── public API ─────────────────────────────────────────────── */

max17048_err_t max17048_init(max17048_t *dev, const max17048_i2c_t *i2c)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    if (!dev || !i2c || !i2c->read_regs || !i2c->write_reg)
        return MAX17048_ERR_PARAM;
    dev->i2c = i2c;

    uint16_t ver;
    if (!rd16(dev, REG_VERSION, &ver)) return MAX17048_ERR_I2C;
    if (ver == 0x0000 || ver == 0xFFFF) return MAX17048_ERR_ID;

    return MAX17048_OK;
}

max17048_err_t max17048_read(max17048_t *dev, max17048_data_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    if (!dev || !out) return MAX17048_ERR_PARAM;

    uint16_t vcell, soc, crate;
    if (!rd16(dev, REG_VCELL, &vcell))  return MAX17048_ERR_I2C;
    if (!rd16(dev, REG_SOC, &soc))      return MAX17048_ERR_I2C;
    if (!rd16(dev, REG_CRATE, &crate))  return MAX17048_ERR_I2C;

    out->voltage_v   = (float)vcell * 78.125e-6f;
    out->soc_pct     = (float)soc / 256.0f;
    out->charge_rate = (float)(int16_t)crate * 0.208f;

    return MAX17048_OK;
}

max17048_err_t max17048_read_voltage(max17048_t *dev, float *voltage_v)
{
    REQUIRE(dev != NULL);
    REQUIRE(voltage_v != NULL);
    if (!dev || !voltage_v) return MAX17048_ERR_PARAM;
    uint16_t vcell;
    if (!rd16(dev, REG_VCELL, &vcell)) return MAX17048_ERR_I2C;
    *voltage_v = (float)vcell * 78.125e-6f;
    return MAX17048_OK;
}

max17048_err_t max17048_read_soc(max17048_t *dev, float *soc_pct)
{
    REQUIRE(dev != NULL);
    REQUIRE(soc_pct != NULL);
    if (!dev || !soc_pct) return MAX17048_ERR_PARAM;
    uint16_t soc;
    if (!rd16(dev, REG_SOC, &soc)) return MAX17048_ERR_I2C;
    *soc_pct = (float)soc / 256.0f;
    return MAX17048_OK;
}

max17048_err_t max17048_quickstart(max17048_t *dev)
{
    REQUIRE(dev != NULL);
    if (!dev) return MAX17048_ERR_PARAM;
    return wr16(dev, REG_MODE, 0x4000) ? MAX17048_OK : MAX17048_ERR_I2C;
}

max17048_err_t max17048_set_alert_threshold(max17048_t *dev, uint8_t threshold_pct)
{
    REQUIRE(dev != NULL);
    REQUIRE(threshold_pct > 0 && threshold_pct <= 32);
    if (!dev) return MAX17048_ERR_PARAM;
    if (threshold_pct == 0 || threshold_pct > 32) return MAX17048_ERR_PARAM;

    uint16_t cfg;
    if (!rd16(dev, REG_CONFIG, &cfg)) return MAX17048_ERR_I2C;
    cfg = (uint16_t)((cfg & 0xFFE0u) | (32u - threshold_pct));
    return wr16(dev, REG_CONFIG, cfg) ? MAX17048_OK : MAX17048_ERR_I2C;
}

max17048_err_t max17048_clear_alert(max17048_t *dev)
{
    REQUIRE(dev != NULL);
    if (!dev) return MAX17048_ERR_PARAM;
    uint16_t status;
    if (!rd16(dev, REG_STATUS, &status)) return MAX17048_ERR_I2C;
    if (!wr16(dev, REG_STATUS, (uint16_t)(status & ~STATUS_FLAGS)))
        return MAX17048_ERR_I2C;
    uint16_t cfg;
    if (!rd16(dev, REG_CONFIG, &cfg)) return MAX17048_ERR_I2C;
    cfg &= (uint16_t)~(1u << 5);
    return wr16(dev, REG_CONFIG, cfg) ? MAX17048_OK : MAX17048_ERR_I2C;
}

max17048_err_t max17048_sleep(max17048_t *dev)
{
    REQUIRE(dev != NULL);
    if (!dev) return MAX17048_ERR_PARAM;
    /* CONFIG.SLEEP acts only once MODE.EnSleep is set */
    if (!wr16(dev, REG_MODE, MODE_EN_SLEEP)) return MAX17048_ERR_I2C;
    uint16_t cfg;
    if (!rd16(dev, REG_CONFIG, &cfg)) return MAX17048_ERR_I2C;
    cfg = (uint16_t)(cfg | (1u << 7));
    return wr16(dev, REG_CONFIG, cfg) ? MAX17048_OK : MAX17048_ERR_I2C;
}

max17048_err_t max17048_wake(max17048_t *dev)
{
    REQUIRE(dev != NULL);
    if (!dev) return MAX17048_ERR_PARAM;
    uint16_t cfg;
    if (!rd16(dev, REG_CONFIG, &cfg)) return MAX17048_ERR_I2C;
    cfg &= (uint16_t)~(1u << 7);
    return wr16(dev, REG_CONFIG, cfg) ? MAX17048_OK : MAX17048_ERR_I2C;
}

max17048_err_t max17048_version(max17048_t *dev, uint16_t *ver)
{
    REQUIRE(dev != NULL);
    REQUIRE(ver != NULL);
    if (!dev || !ver) return MAX17048_ERR_PARAM;
    return rd16(dev, REG_VERSION, ver) ? MAX17048_OK : MAX17048_ERR_I2C;
}
