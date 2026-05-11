// SPDX-License-Identifier: MIT
/*
 * ms5611 — TE Connectivity MS5611 precision barometric pressure sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation from MS5611-01BA03 datasheet (DA5611-01BA03_011),
 * Section "Compensated values" including 2nd-order correction.
 *
 * MS5611 uses a command-based protocol:
 *   write_reg(ctx, addr, cmd, 0) sends command byte
 *   read_regs(ctx, addr, 0, buf, len) reads result
 */

#include "ms5611.h"
#include "contract.h"
#include <string.h>

/* ---------- command bytes (from datasheet Table 2) ---------- */

#define CMD_RESET       0x1Eu
#define CMD_ADC_READ    0x00u
#define CMD_PROM_BASE   0xA0u   /* PROM read: 0xA0, 0xA2, ..., 0xAE */

/* D1 (pressure) conversion commands by OSR */
#define CMD_D1_BASE     0x40u   /* +0, +2, +4, +6, +8 for OSR 256..4096 */

/* D2 (temperature) conversion commands by OSR */
#define CMD_D2_BASE     0x50u   /* +0, +2, +4, +6, +8 for OSR 256..4096 */

/* Conversion delay in ms by OSR (from datasheet Table 3) */
static const uint8_t osr_delay[] = { 1, 2, 3, 5, 10 };

/* ---------- helpers ---------- */

static bool send_cmd(const ms5611_t *dev, uint8_t cmd)
{
    /* Use write_reg with val=0 as a command send mechanism */
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, cmd, 0);
}

static bool read_bytes(const ms5611_t *dev, uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, 0, buf, len);
}

static void delay(const ms5611_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms) {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

static ms5611_err_t read_prom(ms5611_t *dev)
{
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t cmd = (uint8_t)(CMD_PROM_BASE + (i * 2u));
        uint8_t buf[2];

        if (!send_cmd(dev, cmd))
            return MS5611_ERR_I2C;
        if (!read_bytes(dev, buf, 2))
            return MS5611_ERR_I2C;

        dev->prom[i] = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    }
    return MS5611_OK;
}

static ms5611_err_t read_adc(ms5611_t *dev, uint8_t conv_cmd, uint32_t *result)
{
    if (!send_cmd(dev, conv_cmd))
        return MS5611_ERR_I2C;

    /* Wait for conversion */
    uint8_t osr = (uint8_t)dev->cfg.osr;
    delay(dev, osr_delay[osr]);

    /* Read ADC result (3 bytes) */
    if (!send_cmd(dev, CMD_ADC_READ))
        return MS5611_ERR_I2C;

    uint8_t buf[3];
    if (!read_bytes(dev, buf, 3))
        return MS5611_ERR_I2C;

    *result = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    return MS5611_OK;
}

ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return MS5611_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg)
        return MS5611_ERR_PARAM;
    if ((unsigned)cfg->osr > 4)
        return MS5611_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Reset */
    if (!send_cmd(dev, CMD_RESET))
        return MS5611_ERR_I2C;
    delay(dev, 3);  /* datasheet: reload takes 2.8ms max */

    /* Read PROM calibration data */
    ms5611_err_t err = read_prom(dev);
    if (err != MS5611_OK)
        return err;

    INVARIANT(dev->i2c != NULL);
    return MS5611_OK;
}

ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out)
{
    if (!dev || !out)
        return MS5611_ERR_PARAM;

    uint8_t osr2 = (uint8_t)(dev->cfg.osr * 2u);

    /* Read raw pressure (D1) and temperature (D2) */
    uint32_t d1 = 0, d2 = 0;
    ms5611_err_t err;

    err = read_adc(dev, (uint8_t)(CMD_D1_BASE + osr2), &d1);
    if (err != MS5611_OK) return err;

    err = read_adc(dev, (uint8_t)(CMD_D2_BASE + osr2), &d2);
    if (err != MS5611_OK) return err;

    /* --- 1st order compensation (datasheet Section "Compensated values") --- */
    uint16_t c1 = dev->prom[1];
    uint16_t c2 = dev->prom[2];
    uint16_t c3 = dev->prom[3];
    uint16_t c4 = dev->prom[4];
    uint16_t c5 = dev->prom[5];
    uint16_t c6 = dev->prom[6];

    int64_t dt   = (int64_t)d2 - ((int64_t)c5 << 8);
    int64_t temp = 2000 + ((dt * (int64_t)c6) >> 23);

    int64_t off  = ((int64_t)c2 << 16) + (((int64_t)c4 * dt) >> 7);
    int64_t sens = ((int64_t)c1 << 15) + (((int64_t)c3 * dt) >> 8);

    /* --- 2nd order correction --- */
    int64_t t2    = 0;
    int64_t off2  = 0;
    int64_t sens2 = 0;

    if (temp < 2000) {
        int64_t t_diff = temp - 2000;
        t2    = (dt * dt) >> 31;
        off2  = (5 * t_diff * t_diff) >> 1;
        sens2 = (5 * t_diff * t_diff) >> 2;

        if (temp < -1500) {
            int64_t t_diff2 = temp + 1500;
            off2  += 7 * t_diff2 * t_diff2;
            sens2 += (11 * t_diff2 * t_diff2) >> 1;
        }
    }

    temp -= t2;
    off  -= off2;
    sens -= sens2;

    int64_t p = (((int64_t)d1 * sens) >> 21) - off;
    p >>= 15;

    out->temperature_c = (float)temp / 100.0f;
    /* p is in units of 0.01 mbar. 1 mbar = 100 Pa, so p * 0.01 * 100 = p Pa */
    out->pressure_pa   = (float)p;

    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */
    ENSURE(out->pressure_pa == out->pressure_pa);
    return MS5611_OK;
}
