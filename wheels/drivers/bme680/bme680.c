// SPDX-License-Identifier: MIT
/*
 * bme680 — Bosch BME680 environmental sensor driver (temp/hum/pres/gas)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Compensation algorithms from Bosch BME680 datasheet (BST-BME680-DS001-00).
 * Integer arithmetic for temperature/pressure/humidity, simplified gas
 * resistance using lookup tables from Bosch reference code.
 */

#include "bme680.h"
#include "contract.h"
#include <string.h>

/* --- register addresses --- */
#define REG_COEFF1_START  0x89u  /* 0x89..0xA1 (25 bytes) */
#define REG_COEFF1_LEN    25u
#define REG_COEFF2_START  0xE1u  /* 0xE1..0xF0 (16 bytes) */
#define REG_COEFF2_LEN    16u
#define REG_CHIP_ID       0xD0u
#define REG_RESET         0xE0u
#define REG_CTRL_HUM      0x72u
#define REG_CTRL_MEAS     0x74u
#define REG_CTRL_GAS1     0x71u
#define REG_GAS_WAIT0     0x64u
#define REG_RES_HEAT0     0x5Au
#define REG_MEAS_STATUS   0x1Du
#define REG_DATA_START    0x1Fu  /* 0x1F..0x2B (13 bytes: pres[3]+temp[3]+hum[2]+pad[3]+gas[2]) */
#define REG_DATA_LEN      13u

#define RESET_CMD         0xB6u
#define MODE_FORCED       0x01u

/* gas range lookup table (from Bosch reference code) */
static const uint32_t gas_range_lut1[16] = {
    2147483647u, 2147483647u, 2147483647u, 2147483647u,
    2147483647u, 2126008810u, 2147483647u, 2130303777u,
    2147483647u, 2147483647u, 2143188679u, 2136746228u,
    2147483647u, 2126008810u, 2147483647u, 2147483647u,
};

static const uint32_t gas_range_lut2[16] = {
    4096000000u, 2048000000u, 1024000000u, 512000000u,
    255744255u,  127110228u,  64000000u,   32258064u,
    16016016u,   8000000u,    4000000u,    2000000u,
    1000000u,    500000u,     250000u,     125000u,
};

/* --- helpers --- */

static bool write_reg(const bme680_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, reg, val);
}

static bool read_regs(const bme680_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, len);
}

static void delay_ms(const bme680_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* --- calibration --- */

static bme680_err_t read_calibration(bme680_t *dev)
{
    uint8_t c1[REG_COEFF1_LEN];
    uint8_t c2[REG_COEFF2_LEN];

    if (!read_regs(dev, REG_COEFF1_START, c1, REG_COEFF1_LEN))
        return BME680_ERR_I2C;
    if (!read_regs(dev, REG_COEFF2_START, c2, REG_COEFF2_LEN))
        return BME680_ERR_I2C;

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
    if (!read_regs(dev, 0x02, &tmp, 1)) return BME680_ERR_I2C;
    dev->res_heat_range = (tmp >> 4) & 0x03;
    if (!read_regs(dev, 0x00, &tmp, 1)) return BME680_ERR_I2C;
    dev->res_heat_val = (int8_t)tmp;
    if (!read_regs(dev, 0x04, &tmp, 1)) return BME680_ERR_I2C;
    dev->range_sw_err = (int8_t)((int8_t)tmp >> 4);

    return BME680_OK;
}

/* --- compensation algorithms (Bosch datasheet integer formulas) --- */

static int32_t compensate_temperature(bme680_t *dev, uint32_t adc_t)
{
    int64_t var1 = ((int32_t)adc_t >> 3) - (int32_t)((uint32_t)dev->par_t1 << 1);
    int64_t var2 = (var1 * (int32_t)dev->par_t2) >> 11;
    int64_t var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = (var3 * (int32_t)((uint32_t)(int32_t)dev->par_t3 << 4)) >> 14;
    dev->t_fine = (int32_t)(var2 + var3);
    return (int32_t)((dev->t_fine * 5 + 128) >> 8);
}

static uint32_t compensate_pressure(const bme680_t *dev, uint32_t adc_p)
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

static uint32_t compensate_humidity(const bme680_t *dev, uint16_t adc_h)
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
    int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    int32_t var6 = (var4 * var5) >> 1;
    uint32_t hum = (uint32_t)((var3 + var6) >> 10);
    /* clamp to 0..100000 (in units of 1/1000 %RH) */
    if (hum > 100000u)
        hum = 100000u;
    return hum;
}

static uint32_t compensate_gas(const bme680_t *dev, uint16_t adc_gas,
                               uint8_t gas_range)
{
    uint8_t r = gas_range & 0x0F;
    int64_t var1 = (int64_t)((1340 + (5 * (int64_t)dev->range_sw_err)) *
                             ((int64_t)gas_range_lut1[r])) >> 16;
    int64_t var2 = (int64_t)adc_gas * 32768 - 16777216 + var1;
    int64_t var3 = ((int64_t)gas_range_lut2[r] * (int64_t)var1) >> 9;

    if (var2 == 0)
        return 0;

    return (uint32_t)((var3 + ((int64_t)var2 >> 1)) / (int64_t)var2);
}

/* --- public API --- */

bme680_err_t bme680_init(bme680_t *dev, const bme680_i2c_t *i2c,
                         uint8_t addr)
{
    if (!dev || !i2c)
        return BME680_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return BME680_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c  = i2c;
    dev->addr = addr;

    /* verify chip ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_CHIP_ID, &id, 1))
        return BME680_ERR_I2C;
    if (id != BME680_CHIP_ID)
        return BME680_ERR_ID;

    /* read calibration data */
    bme680_err_t err = read_calibration(dev);
    if (err != BME680_OK)
        return err;

    /* default oversampling: 1x all, humidity must be written before ctrl_meas */
    if (!write_reg(dev, REG_CTRL_HUM, 0x01))
        return BME680_ERR_I2C;
    if (!write_reg(dev, REG_CTRL_MEAS, (0x01 << 5) | (0x01 << 2)))
        return BME680_ERR_I2C;

    INVARIANT(dev->i2c != NULL);
    return BME680_OK;
}

bme680_err_t bme680_read(bme680_t *dev, bme680_data_t *data)
{
    if (!dev || !data)
        return BME680_ERR_PARAM;

    /* read current oversampling settings */
    uint8_t ctrl_hum = 0;
    uint8_t ctrl_meas = 0;
    if (!read_regs(dev, REG_CTRL_HUM, &ctrl_hum, 1))
        return BME680_ERR_I2C;
    if (!read_regs(dev, REG_CTRL_MEAS, &ctrl_meas, 1))
        return BME680_ERR_I2C;

    /* trigger forced mode: write ctrl_hum then ctrl_meas with mode=forced */
    if (!write_reg(dev, REG_CTRL_HUM, ctrl_hum))
        return BME680_ERR_I2C;
    if (!write_reg(dev, REG_CTRL_MEAS, (uint8_t)((ctrl_meas & 0xFC) | MODE_FORCED)))
        return BME680_ERR_I2C;

    /* wait for measurement completion */
    delay_ms(dev, 50);

    /* poll for new_data bit */
    for (int tries = 0; tries < 100; tries++)
    {
        uint8_t status = 0;
        if (!read_regs(dev, REG_MEAS_STATUS, &status, 1))
            return BME680_ERR_I2C;
        if (status & 0x80)
            break;
        if (tries == 99)
        {
            /* timeout -- data still not ready, proceed with stale data */
            break;
        }
        delay_ms(dev, 10);
    }

    /* read data registers 0x1F..0x2B
     * buf[0..2] = press (0x1F..0x21), buf[3..5] = temp (0x22..0x24),
     * buf[6..7] = hum (0x25..0x26), buf[8..10] = pad (0x27..0x29),
     * buf[11] = gas_r MSB (0x2A), buf[12] = gas_r LSB + valid + range (0x2B)
     */
    uint8_t buf[REG_DATA_LEN];
    if (!read_regs(dev, REG_DATA_START, buf, REG_DATA_LEN))
        return BME680_ERR_I2C;

    /* parse raw ADC values */
    uint32_t adc_pres = ((uint32_t)buf[0] << 12) |
                        ((uint32_t)buf[1] << 4) |
                        ((uint32_t)buf[2] >> 4);
    uint32_t adc_temp = ((uint32_t)buf[3] << 12) |
                        ((uint32_t)buf[4] << 4) |
                        ((uint32_t)buf[5] >> 4);
    uint16_t adc_hum  = (uint16_t)((uint16_t)buf[6] << 8 | buf[7]);
    uint16_t adc_gas  = (uint16_t)((uint16_t)buf[11] << 2 | (buf[12] >> 6));
    uint8_t  gas_range = buf[12] & 0x0F;
    uint8_t  gas_valid = (buf[12] >> 5) & 0x01;

    /* compensate -- temperature must be first (sets t_fine) */
    int32_t temp_comp = compensate_temperature(dev, adc_temp);
    data->temperature_c = (float)temp_comp / 100.0f;

    uint32_t pres_comp = compensate_pressure(dev, adc_pres);
    data->pressure_pa = (float)pres_comp;

    uint32_t hum_comp = compensate_humidity(dev, adc_hum);
    data->humidity_pct = (float)hum_comp / 1000.0f;

    if (gas_valid)
    {
        uint32_t gas_comp = compensate_gas(dev, adc_gas, gas_range);
        data->gas_resistance_ohm = (float)gas_comp;
    }
    else
    {
        data->gas_resistance_ohm = 0.0f;
    }

    ENSURE(data->temperature_c == data->temperature_c);  /* not NaN */
    ENSURE(data->pressure_pa == data->pressure_pa);        /* not NaN */
    ENSURE(data->humidity_pct == data->humidity_pct);      /* not NaN */
    ENSURE(data->temperature_c >= -40.0f && data->temperature_c <= 85.0f);
    ENSURE(data->pressure_pa >= 30000.0f && data->pressure_pa <= 110000.0f);
    ENSURE(data->humidity_pct >= 0.0f && data->humidity_pct <= 100.0f);
    return BME680_OK;
}

bme680_err_t bme680_set_oversampling(bme680_t *dev, uint8_t os_temp,
                                     uint8_t os_hum, uint8_t os_pres)
{
    if (!dev)
        return BME680_ERR_PARAM;
    if (os_temp > 5 || os_hum > 5 || os_pres > 5)
        return BME680_ERR_PARAM;

    if (!write_reg(dev, REG_CTRL_HUM, os_hum & 0x07))
        return BME680_ERR_I2C;
    uint8_t ctrl = (uint8_t)(((os_temp & 0x07) << 5) |
                             ((os_pres & 0x07) << 2));
    if (!write_reg(dev, REG_CTRL_MEAS, ctrl))
        return BME680_ERR_I2C;

    return BME680_OK;
}

bme680_err_t bme680_set_gas_heater(bme680_t *dev, uint16_t temp_c,
                                   uint16_t duration_ms)
{
    if (!dev)
        return BME680_ERR_PARAM;

    /* calculate heater resistance register value */
    /* formula from Bosch BME680 reference code; keep FP intermediates
     * to avoid truncation-to-zero when par_gh2 < 32768 */
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
    uint8_t res_heat = (res_raw <= 0) ? 0u : (res_raw >= 255) ? 255u : (uint8_t)res_raw;

    /* calculate gas wait register value */
    /* duration_ms mapped to multiplication factor encoding */
    uint8_t gas_wait;
    if (duration_ms >= 4032)
    {
        gas_wait = 0xFF;
    }
    else
    {
        uint8_t factor = 0;
        uint16_t dur = duration_ms;
        while (dur > 63)
        {
            dur = dur >> 2;
            factor++;
        }
        gas_wait = (uint8_t)(dur + (factor << 6));
    }

    if (!write_reg(dev, REG_RES_HEAT0, res_heat))
        return BME680_ERR_I2C;
    if (!write_reg(dev, REG_GAS_WAIT0, gas_wait))
        return BME680_ERR_I2C;
    /* enable gas measurement: run_gas=1, nb_conv=0 (heater profile 0) */
    if (!write_reg(dev, REG_CTRL_GAS1, 0x10))
        return BME680_ERR_I2C;

    return BME680_OK;
}

bme680_err_t bme680_reset(bme680_t *dev)
{
    if (!dev)
        return BME680_ERR_PARAM;
    if (!write_reg(dev, REG_RESET, RESET_CMD))
        return BME680_ERR_I2C;
    return BME680_OK;
}
