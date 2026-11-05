// SPDX-License-Identifier: MIT
/*
 * bme688 — Bosch BME688 environmental sensor driver (temp/hum/pres/gas)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithms from Bosch BME688 datasheet (BST-BME688-DS000-18).
 * Register-compatible with BME680; integer arithmetic for temp/press/hum
 * and gas resistance.
 */

#include "bme688.h"
#include "contract.h"
#include <string.h>

/* --- register addresses (from BME688 datasheet) --- */
#define REG_COEFF1_START  0x89u  /* 0x89..0xA1 (25 bytes) */
#define REG_COEFF1_LEN    25u
#define REG_COEFF2_START  0xE1u  /* 0xE1..0xF0 (16 bytes) */
#define REG_COEFF2_LEN    16u
#define REG_CHIP_ID       0xD0u
#define REG_RESET         0xE0u
#define REG_CTRL_HUM      0x72u
#define REG_STATUS        0x73u
#define REG_CTRL_MEAS     0x74u
#define REG_CONFIG        0x75u
#define REG_CTRL_GAS1     0x71u
#define REG_GAS_WAIT0     0x64u
#define REG_RES_HEAT0     0x5Au
#define REG_MEAS_STATUS   0x1Du
#define REG_DATA_START    0x1Fu  /* 0x1F..0x2D (15 bytes) */
#define REG_DATA_LEN      15u

#define RESET_CMD         0xB6u
#define MODE_FORCED       0x01u
#define RUN_GAS           0x20u  /* ctrl_gas_1 run_gas<5> */

/* --- internal helpers --- */

static bool write_reg(const bme688_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const bme688_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, len);
}

static void delay_ms(const bme688_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* --- calibration --- */

static bme688_err_t read_calibration(bme688_t *dev)
{
    uint8_t c1[REG_COEFF1_LEN];
    uint8_t c2[REG_COEFF2_LEN];

    if (!read_regs(dev, REG_COEFF1_START, c1, REG_COEFF1_LEN))
        return BME688_ERR_HAL;
    if (!read_regs(dev, REG_COEFF2_START, c2, REG_COEFF2_LEN))
        return BME688_ERR_HAL;

    /* temperature */
    dev->par_t1 = (uint16_t)((uint16_t)c2[9] << 8 | c2[8]);
    dev->par_t2 = (int16_t)((uint16_t)c1[2] << 8 | c1[1]);
    dev->par_t3 = (int8_t)c1[3];

    /* pressure */
    dev->par_p1  = (uint16_t)((uint16_t)c1[6] << 8 | c1[5]);
    dev->par_p2  = (int16_t)((uint16_t)c1[8] << 8 | c1[7]);
    dev->par_p3  = (int8_t)c1[9];
    dev->par_p4  = (int16_t)((uint16_t)c1[12] << 8 | c1[11]);
    dev->par_p5  = (int16_t)((uint16_t)c1[14] << 8 | c1[13]);
    dev->par_p6  = (int8_t)c1[16];
    dev->par_p7  = (int8_t)c1[15];
    dev->par_p8  = (int16_t)((uint16_t)c1[20] << 8 | c1[19]);
    dev->par_p9  = (int16_t)((uint16_t)c1[22] << 8 | c1[21]);
    dev->par_p10 = c1[23];

    /* humidity */
    dev->par_h1 = (uint16_t)((uint16_t)c2[2] << 4 | (c2[1] & 0x0F));
    dev->par_h2 = (uint16_t)((uint16_t)c2[0] << 4 | (c2[1] >> 4));
    dev->par_h3 = (int8_t)c2[3];
    dev->par_h4 = (int8_t)c2[4];
    dev->par_h5 = (int8_t)c2[5];
    dev->par_h6 = c2[6];
    dev->par_h7 = (int8_t)c2[7];

    /* gas */
    dev->par_gh1 = (int8_t)c2[12];
    dev->par_gh2 = (int16_t)((uint16_t)c2[11] << 8 | c2[10]);
    dev->par_gh3 = (int8_t)c2[13];

    /* extra gas calibration regs */
    uint8_t tmp;
    if (!read_regs(dev, 0x02, &tmp, 1)) return BME688_ERR_HAL;
    dev->res_heat_range = (tmp >> 4) & 0x03;
    if (!read_regs(dev, 0x00, &tmp, 1)) return BME688_ERR_HAL;
    dev->res_heat_val = (int8_t)tmp;
    if (!read_regs(dev, 0x04, &tmp, 1)) return BME688_ERR_HAL;
    dev->range_sw_err = (int8_t)((int8_t)tmp >> 4);

    return BME688_OK;
}

/* --- compensation algorithms (Bosch datasheet integer formulas) --- */

static int32_t compensate_temperature(bme688_t *dev, uint32_t adc_t)
{
    int64_t var1 = ((int32_t)adc_t >> 3) - (int32_t)((uint32_t)dev->par_t1 << 1);
    int64_t var2 = (var1 * (int32_t)dev->par_t2) >> 11;
    int64_t var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = (var3 * (int32_t)((uint32_t)(int32_t)dev->par_t3 << 4)) >> 14;
    dev->t_fine = (int32_t)(var2 + var3);
    return (int32_t)((dev->t_fine * 5 + 128) >> 8);
}

static uint32_t compensate_pressure(const bme688_t *dev, uint32_t adc_p)
{
    int32_t var1 = ((int32_t)dev->t_fine >> 1) - 64000;
    int32_t var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) *
                    (int32_t)dev->par_p6) >> 2;
    var2 = var2 + (int32_t)((uint32_t)(var1 * (int32_t)dev->par_p5) << 1);
    var2 = (var2 >> 2) + (int32_t)((uint32_t)(int32_t)dev->par_p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) *
             (int32_t)((uint32_t)(int32_t)dev->par_p3 << 5)) >> 3) +
           (((int32_t)dev->par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)dev->par_p1) >> 15;

    if (var1 == 0)
        return 0;

    uint32_t press = (uint32_t)(((int32_t)1048576 - (int32_t)adc_p) -
                                (var2 >> 12));
    press = press * 3125u;

    if (press >= (uint32_t)0x40000000u)
        press = (press / (uint32_t)var1) << 1;
    else
        press = (press << 1) / (uint32_t)var1;

    var1 = ((int32_t)dev->par_p9 *
            (int32_t)(((press >> 3) * (press >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(press >> 2) * (int32_t)dev->par_p8) >> 13;
    int64_t p8 = (int64_t)(press >> 8);
    int32_t var3 = (int32_t)((p8 * p8 * p8 * (int64_t)dev->par_p10) >> 17);

    press = (uint32_t)((int32_t)press +
                       ((var1 + var2 + var3 +
                         (int32_t)((uint32_t)(int32_t)dev->par_p7 << 7)) >> 4));
    return press;
}

static uint32_t compensate_humidity(const bme688_t *dev, uint16_t adc_h)
{
    int32_t temp_scaled = (int32_t)((dev->t_fine * 5 + 128) >> 8);
    int32_t var1 = (int32_t)adc_h - (int32_t)((uint32_t)dev->par_h1 << 4) -
                   (((temp_scaled * (int32_t)dev->par_h3) / 100) >> 1);
    int32_t var2 = ((int32_t)dev->par_h2 *
                    (((temp_scaled * (int32_t)dev->par_h4) / 100) +
                     (((temp_scaled *
                        ((temp_scaled * (int32_t)dev->par_h5) / 100)) >> 6) /
                      100) +
                     (1 << 14))) >> 10;
    int32_t var3 = var1 * var2;
    int32_t var4 = (int32_t)((uint32_t)dev->par_h6 << 7);
    var4 = (var4 + ((temp_scaled * (int32_t)dev->par_h7) / 100)) >> 4;
    int32_t var5 = (int32_t)(((int64_t)(var3 >> 14) * (var3 >> 14)) >> 10);
    int32_t var6 = (int32_t)(((int64_t)var4 * var5) >> 1);
    uint32_t hum = (uint32_t)(((((int64_t)var3 + var6) >> 10) * 1000) >> 12);
    /* clamp to 0..100000 (in units of 1/1000 %RH) */
    if (hum > 100000u)
        hum = 100000u;
    return hum;
}

/* adc_gas has 10 bits, so var2 stays between 2560 and 5629 */
static uint32_t compensate_gas(uint16_t adc_gas, uint8_t gas_range)
{
    uint32_t var1 = UINT32_C(262144) >> gas_range;
    int32_t var2 = (int32_t)adc_gas - INT32_C(512);
    var2 *= INT32_C(3);
    var2 = INT32_C(4096) + var2;
    /* times 10000, then 100: multiplying by 1000000 would overflow */
    return ((UINT32_C(10000) * var1) / (uint32_t)var2) * 100u;
}

/* --- public API --- */

bme688_err_t bme688_init(bme688_t *dev, const bme688_i2c_t *i2c,
                         const bme688_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return BME688_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return BME688_ERR_PARAM;
    if (cfg->osrs_t > 5 || cfg->osrs_p > 5 || cfg->osrs_h > 5)
        return BME688_ERR_PARAM;
    if (cfg->filter > 7)
        return BME688_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return BME688_ERR_HAL;
    if (id != BME688_CHIP_ID)
        return BME688_ERR_ID;

    /* read calibration data */
    bme688_err_t err = read_calibration(dev);
    if (err != BME688_OK)
        return err;

    /* configure IIR filter */
    uint8_t config = (uint8_t)((cfg->filter & 0x07) << 2);
    if (!write_reg(dev, REG_CONFIG, config))
        return BME688_ERR_HAL;

    /* set oversampling: humidity must be written before ctrl_meas */
    if (!write_reg(dev, REG_CTRL_HUM, cfg->osrs_h & 0x07))
        return BME688_ERR_HAL;
    /* ctrl_meas: osrs_t[7:5] | osrs_p[4:2] | mode=sleep[1:0] */
    uint8_t ctrl = (uint8_t)(((cfg->osrs_t & 0x07) << 5) |
                             ((cfg->osrs_p & 0x07) << 2));
    if (!write_reg(dev, REG_CTRL_MEAS, ctrl))
        return BME688_ERR_HAL;

    /* configure gas heater if requested */
    if (cfg->gas_heater_dur_ms > 0 && cfg->gas_heater_temp_c > 0)
    {
        err = bme688_set_gas_heater(dev, cfg->gas_heater_temp_c,
                                    cfg->gas_heater_dur_ms);
        if (err != BME688_OK)
            return err;
    }

    INVARIANT(dev->i2c != NULL);
    return BME688_OK;
}

bme688_err_t bme688_read(bme688_t *dev, bme688_data_t *out)
{
    if (!dev || !out)
        return BME688_ERR_PARAM;

    /* read current oversampling settings */
    uint8_t ctrl_hum = 0;
    uint8_t ctrl_meas = 0;
    if (!read_regs(dev, REG_CTRL_HUM, &ctrl_hum, 1))
        return BME688_ERR_HAL;
    if (!read_regs(dev, REG_CTRL_MEAS, &ctrl_meas, 1))
        return BME688_ERR_HAL;

    /* trigger forced mode: write ctrl_hum then ctrl_meas with mode=forced */
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
        return BME688_ERR_HAL;
    if (!write_reg(dev, REG_CTRL_MEAS,
                   (uint8_t)((ctrl_meas & 0xFC) | MODE_FORCED)))
        return BME688_ERR_HAL;

    /* wait for measurement completion */
    delay_ms(dev, 50);

    /* poll for new_data bit (bit 7 of meas_status register) */
    bool data_ready = false;
    for (int tries = 0; tries < 100; tries++)
    {
        uint8_t status = 0;
        if (!read_regs(dev, REG_MEAS_STATUS, &status, 1))
            return BME688_ERR_HAL;
        if (status & 0x80)
        {
            data_ready = true;
            break;
        }
        delay_ms(dev, 10);
    }
    if (!data_ready)
        return BME688_ERR_TIMEOUT;

    /* read field 0 data registers 0x1F..0x2D (15 bytes)
     * buf[0..2]  = pressure (0x1F..0x21, 20-bit)
     * buf[3..5]  = temperature (0x22..0x24, 20-bit)
     * buf[6..7]  = humidity (0x25..0x26, 16-bit)
     * buf[8..12] = padding (0x27..0x2B)
     * buf[13]    = gas_r MSB (0x2C)
     * buf[14]    = gas_r LSB[7:6] | gas_valid[5] | heat_stab[4] | gas_range[3:0] (0x2D)
     */
    uint8_t buf[REG_DATA_LEN];
    if (!read_regs(dev, REG_DATA_START, buf, REG_DATA_LEN))
        return BME688_ERR_HAL;

    /* parse raw ADC values */
    uint32_t adc_pres = ((uint32_t)buf[0] << 12) |
                        ((uint32_t)buf[1] << 4) |
                        ((uint32_t)buf[2] >> 4);
    uint32_t adc_temp = ((uint32_t)buf[3] << 12) |
                        ((uint32_t)buf[4] << 4) |
                        ((uint32_t)buf[5] >> 4);
    uint16_t adc_hum  = (uint16_t)((uint16_t)buf[6] << 8 | buf[7]);
    uint16_t adc_gas  = (uint16_t)((uint16_t)buf[13] << 2 | (buf[14] >> 6));
    uint8_t  gas_range = buf[14] & 0x0F;
    uint8_t  gas_valid = (buf[14] >> 5) & 0x01;

    /* compensate -- temperature must be first (sets t_fine) */
    int32_t temp_comp = compensate_temperature(dev, adc_temp);
    out->temperature_c = (float)temp_comp / 100.0f;

    uint32_t pres_comp = compensate_pressure(dev, adc_pres);
    out->pressure_pa = (float)pres_comp;

    uint32_t hum_comp = compensate_humidity(dev, adc_hum);
    out->humidity_pct = (float)hum_comp / 1000.0f;

    if (gas_valid)
    {
        uint32_t gas_comp = compensate_gas(adc_gas, gas_range);
        out->gas_resistance_ohm = gas_comp;
        out->gas_valid = true;
    }
    else
    {
        out->gas_resistance_ohm = 0;
        out->gas_valid = false;
    }

    ENSURE(out->temperature_c == out->temperature_c);  /* not NaN */
    ENSURE(out->pressure_pa == out->pressure_pa);        /* not NaN */
    ENSURE(out->humidity_pct == out->humidity_pct);      /* not NaN */
    return BME688_OK;
}

bme688_err_t bme688_set_gas_heater(bme688_t *dev, uint16_t temp_c,
                                   uint16_t dur_ms)
{
    if (!dev)
        return BME688_ERR_PARAM;

    /* calculate heater resistance register value
     * formula from Bosch BME680/BME688 reference code */
    int32_t rhr = (int32_t)dev->res_heat_range * 16384;
    int32_t pgh = (int32_t)dev->par_gh1 / 16;
    float var1f = ((float)dev->par_gh2 / 32768.0f) *
                  ((float)rhr + (float)pgh + 49152.0f);
    int32_t var1 = (var1f <= -2147483648.0f) ? (int32_t)-2147483647 - 1
                 : (var1f >=  2147483647.0f) ? (int32_t) 2147483647
                 : (int32_t)var1f;
    int32_t var2 = (var1 / 32768) + 1;
    int32_t var3 = (int32_t)temp_c * var2 / 2;
    int32_t var4 = var3 + (dev->res_heat_val * 16);
    int32_t res_raw = (var4 + 50) / 100;
    uint8_t res_heat = (res_raw <= 0) ? 0u
                     : (res_raw >= 255) ? 255u
                     : (uint8_t)res_raw;

    /* calculate gas wait register value
     * duration_ms mapped to multiplication factor encoding */
    uint8_t gas_wait;
    if (dur_ms >= 4032)
    {
        gas_wait = 0xFF;
    }
    else
    {
        uint8_t factor = 0;
        uint16_t dur = dur_ms;
        while (dur > 63)
        {
            dur = dur >> 2;
            factor++;
        }
        gas_wait = (uint8_t)(dur + (factor << 6));
    }

    if (!write_reg(dev, REG_RES_HEAT0, res_heat))
        return BME688_ERR_HAL;
    if (!write_reg(dev, REG_GAS_WAIT0, gas_wait))
        return BME688_ERR_HAL;
    /* enable gas measurement: run_gas=1, nb_conv=0 (heater profile 0) */
    if (!write_reg(dev, REG_CTRL_GAS1, RUN_GAS))
        return BME688_ERR_HAL;

    return BME688_OK;
}

bme688_err_t bme688_reset(bme688_t *dev)
{
    if (!dev)
        return BME688_ERR_PARAM;
    if (!write_reg(dev, REG_RESET, RESET_CMD))
        return BME688_ERR_HAL;
    return BME688_OK;
}
