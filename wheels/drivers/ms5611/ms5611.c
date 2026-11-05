// SPDX-License-Identifier: MIT
/*
 * ms5611 — TE Connectivity MS5611 precision barometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithm from MS5611-01BA03 datasheet, including
 * 2nd-order compensation for T < 20C and T < -15C.
 */

#include "ms5611.h"
#include "contract.h"
#include <string.h>

/* Commands */
#define CMD_RESET    0x1E
#define CMD_ADC_READ 0x00
#define CMD_PROM_BASE 0xA0

/* Convert commands base by OSR: D1=0x40+osr*2, D2=0x50+osr*2 */
#define CMD_D1_BASE  0x40
#define CMD_D2_BASE  0x50

/* Delay in ms per OSR level: 256->1ms, 512->2ms, 1024->3ms, 2048->5ms, 4096->10ms */
static const uint8_t osr_delay[] = { 1, 2, 3, 5, 10 };

static bool send_cmd(const ms5611_t *dev, uint8_t cmd)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, cmd, 0);
}

static bool read_bytes(const ms5611_t *dev, uint8_t cmd, uint8_t *buf,
                       uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, cmd, buf, len);
}

static void delay(const ms5611_t *dev, uint32_t ms)
{
    dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

/* CRC-4 verification per MS5611 datasheet AN520 */
static uint8_t prom_crc4(const uint16_t prom[8])
{
    uint32_t rem = 0;
    uint16_t data[8];
    uint8_t i, bit;

    for (i = 0; i < 8; i++) {
        data[i] = prom[i];
    }

    /* Word 7 enters the calculation with its whole low byte cleared */
    data[7] = (uint16_t)(data[7] & 0xFF00);

    for (i = 0; i < 16; i++) {
        if (i % 2 == 1) {
            rem ^= (uint32_t)(data[i >> 1] & 0x00FF);
        } else {
            rem ^= (uint32_t)(data[i >> 1] >> 8);
        }
        for (bit = 8; bit > 0; bit--) {
            if (rem & 0x8000U) {
                rem = (rem << 1) ^ 0x3000U;
            } else {
                rem <<= 1;
            }
        }
    }
    return (uint8_t)((rem >> 12) & 0x000F);
}

static ms5611_err_t read_prom(ms5611_t *dev)
{
    uint16_t prom[8];
    uint8_t buf[2];
    int i;

    /* Read all 8 PROM words (0xA0..0xAE) for CRC validation */
    for (i = 0; i < 8; i++)
    {
        uint8_t cmd = (uint8_t)(CMD_PROM_BASE + (uint8_t)(i * 2));
        if (!read_bytes(dev, cmd, buf, 2))
        {
            return MS5611_ERR_I2C;
        }
        prom[i] = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    }

    /* Verify CRC-4 (DB3-DB0 of word 7, Figure 16) */
    uint8_t crc_read = (uint8_t)(prom[7] & 0x000F);
    uint8_t crc_calc = prom_crc4(prom);
    if (crc_read != crc_calc)
    {
        return MS5611_ERR_CRC;
    }

    /* Extract calibration coefficients (C1-C6 = words 1-6) */
    dev->calib.c1 = prom[1];
    dev->calib.c2 = prom[2];
    dev->calib.c3 = prom[3];
    dev->calib.c4 = prom[4];
    dev->calib.c5 = prom[5];
    dev->calib.c6 = prom[6];

    return MS5611_OK;
}

static ms5611_err_t read_adc(const ms5611_t *dev, uint8_t conv_cmd,
                             uint32_t *adc_val)
{
    if (!send_cmd(dev, conv_cmd))
    {
        return MS5611_ERR_I2C;
    }

    uint8_t osr_idx = (uint8_t)dev->cfg.osr;
    if (osr_idx > 4) osr_idx = 4;
    delay(dev, osr_delay[osr_idx]);

    uint8_t buf[3];
    if (!read_bytes(dev, CMD_ADC_READ, buf, 3))
    {
        return MS5611_ERR_I2C;
    }

    *adc_val = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    return MS5611_OK;
}

ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
    {
        return MS5611_ERR_PARAM;
    }

    if (!i2c->delay_ms)
    {
        return MS5611_ERR_PARAM;
    }

    if ((unsigned)cfg->osr > 4)
    {
        return MS5611_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Send reset command */
    if (!send_cmd(dev, CMD_RESET))
    {
        return MS5611_ERR_I2C;
    }
    delay(dev, 3);

    /* Read PROM calibration data */
    ms5611_err_t err = read_prom(dev);
    if (err != MS5611_OK)
    {
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return MS5611_OK;
}

ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out)
{
    if (!dev || !out)
    {
        return MS5611_ERR_PARAM;
    }

    uint8_t osr_idx = (uint8_t)dev->cfg.osr;
    if (osr_idx > 4) osr_idx = 4;

    /* Read D1 (pressure) */
    uint32_t d1 = 0;
    uint8_t d1_cmd = (uint8_t)(CMD_D1_BASE + osr_idx * 2);
    ms5611_err_t err = read_adc(dev, d1_cmd, &d1);
    if (err != MS5611_OK)
    {
        return err;
    }

    /* Read D2 (temperature) */
    uint32_t d2 = 0;
    uint8_t d2_cmd = (uint8_t)(CMD_D2_BASE + osr_idx * 2);
    err = read_adc(dev, d2_cmd, &d2);
    if (err != MS5611_OK)
    {
        return err;
    }

    /* 1st order compensation (datasheet formulas) */
    int64_t dT = (int64_t)d2 - ((int64_t)dev->calib.c5 * 256);
    int64_t temp = 2000 + (dT * (int64_t)dev->calib.c6) / 8388608;

    int64_t off = (int64_t)dev->calib.c2 * 65536 +
                  ((int64_t)dev->calib.c4 * dT) / 128;
    int64_t sens = (int64_t)dev->calib.c1 * 32768 +
                   ((int64_t)dev->calib.c3 * dT) / 256;

    /* 2nd order compensation */
    if (temp < 2000)
    {
        int64_t t2 = (dT * dT) / 2147483648LL;
        int64_t off2 = 5 * ((temp - 2000) * (temp - 2000)) / 2;
        int64_t sens2 = 5 * ((temp - 2000) * (temp - 2000)) / 4;

        if (temp < -1500)
        {
            off2 += 7 * ((temp + 1500) * (temp + 1500));
            sens2 += 11 * ((temp + 1500) * (temp + 1500)) / 2;
        }

        temp -= t2;
        off -= off2;
        sens -= sens2;
    }

    int64_t p = ((int64_t)d1 * sens / 2097152 - off) / 32768;

    out->temperature_c = (float)temp / 100.0f;
    out->pressure_pa = (float)p;

    ENSURE(out->temperature_c == out->temperature_c);
    ENSURE(out->pressure_pa == out->pressure_pa);
    return MS5611_OK;
}

ms5611_err_t ms5611_reset(ms5611_t *dev)
{
    if (!dev)
    {
        return MS5611_ERR_PARAM;
    }
    if (!send_cmd(dev, CMD_RESET))
    {
        return MS5611_ERR_I2C;
    }
    delay(dev, 3);
    return MS5611_OK;
}
