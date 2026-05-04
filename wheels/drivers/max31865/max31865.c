// SPDX-License-Identifier: MIT
/*
 * max31865 — SPI RTD-to-digital converter for PT100/PT1000
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Register map: config 0x00, RTD MSB 0x01, RTD LSB 0x02, fault 0x07.
 * Read address = reg, write address = reg | 0x80.
 * Temperature conversion uses Callendar-Van Dusen equation for T >= 0C
 * and a rational approximation for T < 0C.
 */

#include "max31865.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_CONFIG      0x00
#define REG_RTD_MSB     0x01
#define REG_RTD_LSB     0x02
#define REG_FAULT       0x07

#define WRITE_BIT       0x80

/* ---------- config register bits ---------- */

#define CFG_BIAS        0x80
#define CFG_AUTO        0x40
#define CFG_CLEAR_FAULT 0x02
#define CFG_3WIRE       0x10
#define CFG_50HZ        0x01

/* ---------- Callendar-Van Dusen coefficients (IEC 60751) ---------- */

#define CVD_A   3.9083e-3f
#define CVD_B  -5.775e-7f

/* ---------- SPI helpers ---------- */

static uint8_t read_reg(const max31865_t *dev, uint8_t reg)
{
    dev->spi.cs_low(dev->spi.ctx);
    dev->spi.xfer(dev->spi.ctx, reg);          /* address byte, bit7=0 */
    uint8_t val = dev->spi.xfer(dev->spi.ctx, 0xFF);
    dev->spi.cs_high(dev->spi.ctx);
    return val;
}

static void write_reg(const max31865_t *dev, uint8_t reg, uint8_t val)
{
    dev->spi.cs_low(dev->spi.ctx);
    dev->spi.xfer(dev->spi.ctx, (uint8_t)(reg | WRITE_BIT));
    dev->spi.xfer(dev->spi.ctx, val);
    dev->spi.cs_high(dev->spi.ctx);
}

/* ---------- temperature math ---------- */

static float rtd_to_temperature(float r_rtd, float r_nom)
{
    float ratio = r_rtd / r_nom;

    /* Callendar-Van Dusen: R = R0 * (1 + A*T + B*T^2)
     * Quadratic solution for T >= 0C */
    float disc = CVD_A * CVD_A - 4.0f * CVD_B * (1.0f - ratio);
    if (disc < 0.0f) {
        /* Below ~ -240C; clamp to avoid sqrt of negative */
        return -240.0f;
    }

    /* Use sqrtf approximation — Newton's method, 3 iterations */
    float x = disc;
    float g = disc * 0.5f;
    if (g <= 0.0f) g = 1.0f;
    g = (g + x / g) * 0.5f;
    g = (g + x / g) * 0.5f;
    g = (g + x / g) * 0.5f;

    float temp = (-CVD_A + g) / (2.0f * CVD_B);

    /* For T < 0C the quadratic overestimates.  Use linear approximation
     * T ~ (R/R0 - 1) / A.  Accuracy is ~+/-0.3C down to -50C; below that
     * the cubic Callendar-Van Dusen term (C coefficient) would be needed,
     * but a full cubic solver exceeds the scope of this lightweight driver. */
    if (temp < 0.0f) {
        temp = (ratio - 1.0f) / CVD_A;
    }

    return temp;
}

/* ---------- API ---------- */

max31865_err_t max31865_init(max31865_t *dev, const max31865_spi_t *spi,
                             const max31865_cfg_t *cfg)
{
    if (!dev || !spi || !cfg)
        return MAX31865_ERR_PARAM;

    if (!spi->cs_low || !spi->cs_high || !spi->xfer)
        return MAX31865_ERR_SPI;

    if (!(cfg->r_ref > 0.0f) || !(cfg->r_nom > 0.0f))
        return MAX31865_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->spi = *spi;
    dev->cfg = *cfg;

    /* Build config byte: bias on + auto-conversion + wire + filter */
    uint8_t config = CFG_BIAS | CFG_AUTO;
    if (cfg->wire == MAX31865_WIRE_3)
        config |= CFG_3WIRE;
    if (cfg->filter_50hz)
        config |= CFG_50HZ;

    write_reg(dev, REG_CONFIG, config);

    return MAX31865_OK;
}

max31865_err_t max31865_read_rtd_raw(max31865_t *dev, uint16_t *rtd_raw)
{
    if (!dev || !rtd_raw)
        return MAX31865_ERR_PARAM;

    uint8_t msb = read_reg(dev, REG_RTD_MSB);
    uint8_t lsb = read_reg(dev, REG_RTD_LSB);

    /* Bit 0 of LSB is fault flag */
    if (lsb & 0x01)
        return MAX31865_ERR_FAULT;

    *rtd_raw = (uint16_t)(((uint16_t)msb << 8 | lsb) >> 1);

    return MAX31865_OK;
}

max31865_err_t max31865_read_temperature(max31865_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return MAX31865_ERR_PARAM;

    uint16_t rtd_raw;
    max31865_err_t err = max31865_read_rtd_raw(dev, &rtd_raw);
    if (err != MAX31865_OK)
        return err;

    /* Convert ADC code to resistance: R_rtd = (rtd_raw / 32768) * R_ref */
    float r_rtd = ((float)rtd_raw / 32768.0f) * dev->cfg.r_ref;

    *temp_c = rtd_to_temperature(r_rtd, dev->cfg.r_nom);

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    ENSURE(*temp_c >= -240.0f && *temp_c <= 850.0f);
    return MAX31865_OK;
}

max31865_err_t max31865_read_fault(max31865_t *dev, uint8_t *fault)
{
    if (!dev || !fault)
        return MAX31865_ERR_PARAM;

    *fault = read_reg(dev, REG_FAULT);

    return MAX31865_OK;
}

max31865_err_t max31865_clear_fault(max31865_t *dev)
{
    if (!dev)
        return MAX31865_ERR_PARAM;

    /* Read current config, set fault-clear bit, write back */
    uint8_t cfg = read_reg(dev, REG_CONFIG);
    cfg |= CFG_CLEAR_FAULT;
    /* Clear the auto-start bit momentarily during fault clear per datasheet */
    cfg &= (uint8_t)~CFG_AUTO;
    write_reg(dev, REG_CONFIG, cfg);

    /* Re-enable auto conversion */
    cfg &= (uint8_t)~CFG_CLEAR_FAULT;
    cfg |= CFG_AUTO;
    write_reg(dev, REG_CONFIG, cfg);

    return MAX31865_OK;
}
