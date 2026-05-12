// SPDX-License-Identifier: MIT
/*
 * air_quality -- integration recipe: air quality monitoring station
 *
 * Demonstrates six L1 driver wheels working together:
 *   L1  ccs811     -- eCO2 + TVOC gas sensor
 *   L1  sgp40      -- VOC raw signal sensor
 *   L1  bme680     -- temperature + pressure + humidity + gas resistance
 *   L1  sht40      -- precision temperature + humidity
 *   L1  aht20      -- budget temperature + humidity
 *   L1  veml7700   -- ambient light sensor
 *
 * Integration flow:
 *   1. Mock I2C register arrays / command buffers for all 6 sensors
 *   2. Initialize all sensors
 *   3. Read each sensor individually
 *   4. Cross-validate: temperature from BME680 vs SHT40 vs AHT20
 *   5. Compute simple Air Quality Index from combined readings
 *
 * Runs entirely on host with mock I2C -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/air_quality
 */

#include "test_harness.h"

/* wheels */
#include "ccs811.h"
#include "sgp40.h"
#include "bme680.h"
#include "sht40.h"
#include "aht20.h"
#include "veml7700.h"

#include <string.h>
#include <math.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define TEMP_CROSS_TOLERANCE  3.0f   /* max delta between sensors (C)     */

/* Simple AQI thresholds */
#define AQI_GOOD       50
#define AQI_MODERATE  100
#define AQI_POOR      150
#define AQI_UNHEALTHY 200

/* ======================================================================
 * Mock I2C -- CCS811 (register-based: write_reg / read_regs)
 *
 * Registers:
 *   0x00 STATUS: bit[4]=APP_VALID, bit[7]=FW_MODE, bit[3]=DATA_READY
 *   0x01 MEAS_MODE
 *   0x02 ALG_RESULT: 4 bytes (eCO2 MSB, eCO2 LSB, TVOC MSB, TVOC LSB)
 *   0x20 HW_ID: 0x81
 *   0xF4 APP_START
 * ====================================================================== */

static uint8_t ccs_regs[256];

static void ccs_mock_reset(void)
{
    memset(ccs_regs, 0, sizeof(ccs_regs));

    /* Hardware ID */
    ccs_regs[0x20] = CCS811_HW_ID;

    /* STATUS: APP_VALID=1, FW_MODE=1, DATA_READY=1 */
    ccs_regs[0x00] = (1u << 4) | (1u << 7) | (1u << 3);

    /* Plant eCO2=650 ppm, TVOC=42 ppb */
    ccs_regs[0x02] = (uint8_t)(650 >> 8);
    ccs_regs[0x03] = (uint8_t)(650 & 0xFF);
    ccs_regs[0x04] = (uint8_t)(42 >> 8);
    ccs_regs[0x05] = (uint8_t)(42 & 0xFF);
}

static bool ccs_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ccs_regs[reg] = val;
    return true;
}

static bool ccs_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = ccs_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const ccs811_i2c_t ccs_i2c = {
    .write_reg = ccs_write_reg,
    .read_regs = ccs_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- SGP40 (raw write/read, command-based)
 *
 * Init: self-test cmd 0x280E -> response 0xD400 + CRC
 * Measure: cmd 0x260F + comp data -> sraw + CRC
 * ====================================================================== */

static uint8_t sgp_read_buf[3];
static uint8_t sgp_last_cmd[2];

static void sgp_mock_reset(void)
{
    memset(sgp_read_buf, 0, sizeof(sgp_read_buf));
    memset(sgp_last_cmd, 0, sizeof(sgp_last_cmd));

    /* Default: self-test OK response (0xD400 + CRC) */
    sgp_read_buf[0] = 0xD4;
    sgp_read_buf[1] = 0x00;
    sgp_read_buf[2] = sgp40_crc8(sgp_read_buf, 2);
}

static void sgp_plant_sraw(uint16_t sraw)
{
    sgp_read_buf[0] = (uint8_t)(sraw >> 8);
    sgp_read_buf[1] = (uint8_t)(sraw & 0xFF);
    sgp_read_buf[2] = sgp40_crc8(sgp_read_buf, 2);
}

static bool sgp_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                      uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (len >= 2) {
        sgp_last_cmd[0] = data[0];
        sgp_last_cmd[1] = data[1];
        /* After self-test is consumed by init, switch to measurement response */
        if (data[0] == 0x26 && data[1] == 0x0F) {
            /* Measure command: plant a realistic sraw value */
            sgp_plant_sraw(28000);
        }
    }
    return true;
}

static bool sgp_read(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len && i < sizeof(sgp_read_buf); i++) {
        data[i] = sgp_read_buf[i];
    }
    return true;
}

static const sgp40_i2c_t sgp_i2c = {
    .write    = sgp_write,
    .read     = sgp_read,
    .delay_ms = NULL,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock I2C -- BME680 (register-based: write_reg / read_regs)
 *
 * Registers:
 *   0xD0 CHIP_ID: 0x61
 *   0x89..0xA1 calibration bank 1 (25 bytes)
 *   0xE1..0xF0 calibration bank 2 (16 bytes)
 *   0x02 res_heat_range, 0x00 res_heat_val, 0x04 range_sw_err
 *   0x1D MEAS_STATUS: bit[7]=new_data
 *   0x1F..0x2B data registers (13 bytes)
 * ====================================================================== */

static uint8_t bme_regs[256];

static void bme_mock_reset(void)
{
    memset(bme_regs, 0, sizeof(bme_regs));

    /* Chip ID */
    bme_regs[0xD0] = BME680_CHIP_ID;

    /*
     * Calibration bank 1 (0x89..0xA1) -- 25 bytes at indices c1[0..24]
     * The driver reads: par_t2 = c1[2:1], par_t3 = c1[3]
     *   par_p1 = c1[6:5], par_p2 = c1[8:7], par_p3 = c1[9],
     *   par_p4 = c1[12:11], par_p5 = c1[14:13], par_p6 = c1[16],
     *   par_p7 = c1[15], par_p8 = c1[20:19], par_p9 = c1[22:21],
     *   par_p10 = c1[23]
     * Use realistic values producing ~25C, ~50%RH, ~101325 Pa.
     */
    uint8_t c1[25];
    memset(c1, 0, sizeof(c1));
    /* par_t2 = 26435 = 0x6743 at c1[1..2] */
    c1[1] = 0x43; c1[2] = 0x67;
    /* par_t3 = 3 at c1[3] */
    c1[3] = 0x03;
    /* par_p1 = 36477 = 0x8E7D at c1[5..6] */
    c1[5] = 0x7D; c1[6] = 0x8E;
    /* par_p2 = -10685 = 0xD643 at c1[7..8] */
    c1[7] = 0x43; c1[8] = 0xD6;
    /* par_p3 = 88 at c1[9] */
    c1[9] = 88;
    /* par_p4 = 7243 at c1[11..12] */
    c1[11] = (uint8_t)(7243 & 0xFF); c1[12] = (uint8_t)(7243 >> 8);
    /* par_p5 = 140 at c1[13..14] */
    c1[13] = (uint8_t)(140 & 0xFF); c1[14] = (uint8_t)(140 >> 8);
    /* par_p7 = 9 at c1[15] (note: p7 is before p6 in ordering!) */
    c1[15] = 9;
    /* par_p6 = -7 at c1[16] */
    c1[16] = (uint8_t)(int8_t)-7;
    /* par_p8 = -10240 at c1[19..20] */
    c1[19] = (uint8_t)(-10240 & 0xFF); c1[20] = (uint8_t)((-10240 >> 8) & 0xFF);
    /* par_p9 = -7396 at c1[21..22] */
    c1[21] = (uint8_t)(-7396 & 0xFF); c1[22] = (uint8_t)((-7396 >> 8) & 0xFF);
    /* par_p10 = 30 at c1[23] */
    c1[23] = 30;
    memcpy(&bme_regs[0x89], c1, 25);

    /*
     * Calibration bank 2 (0xE1..0xF0) -- 16 bytes at indices c2[0..15]
     * The driver reads: par_t1 = c2[9:8], par_h1 = c2[2]<<4 | (c2[1]&0x0F),
     *   par_h2 = c2[0]<<4 | (c2[1]>>4), par_h3..h7 = c2[3..7],
     *   par_gh2 = c2[11:10], par_gh1 = c2[12], par_gh3 = c2[13]
     */
    uint8_t c2[16];
    memset(c2, 0, sizeof(c2));
    /* par_t1 = 27504 = 0x6B70 at c2[8..9] */
    c2[8] = 0x70; c2[9] = 0x6B;
    /* par_h2 = 819 -> c2[0] = 819>>4 = 51, c2[1] upper nibble = 819&0x0F = 3 */
    /* par_h1 = 698 -> c2[2] = 698>>4 = 43, c2[1] lower nibble = 698&0x0F = 10 */
    c2[0] = 51;
    c2[1] = (uint8_t)((3 << 4) | 10);   /* upper=par_h2 low, lower=par_h1 low */
    c2[2] = 43;
    /* par_h3 = 0, par_h4 = 45, par_h5 = 20, par_h6 = 120, par_h7 = -100 */
    c2[3] = 0;
    c2[4] = 45;
    c2[5] = 20;
    c2[6] = 120;
    c2[7] = (uint8_t)(int8_t)-100;
    /* par_gh2 = -4096 at c2[10..11] */
    c2[10] = (uint8_t)(-4096 & 0xFF); c2[11] = (uint8_t)((-4096 >> 8) & 0xFF);
    /* par_gh1 = -25 at c2[12] */
    c2[12] = (uint8_t)(int8_t)-25;
    /* par_gh3 = -7 at c2[13] */
    c2[13] = (uint8_t)(int8_t)-7;
    memcpy(&bme_regs[0xE1], c2, 16);

    /* Extra gas calibration regs */
    bme_regs[0x02] = (1 << 4);  /* res_heat_range = 1 */
    bme_regs[0x00] = 50;        /* res_heat_val = 50 */
    bme_regs[0x04] = 0x10;      /* range_sw_err = 1 (bits[7:4], signed) */

    /* MEAS_STATUS: new_data=1 */
    bme_regs[0x1D] = 0x80;

    /*
     * Data registers at 0x1F..0x2B (13 bytes):
     *   [0..2] = pressure (20-bit ADC << 4)
     *   [3..5] = temperature (20-bit ADC << 4)
     *   [6..7] = humidity (16-bit ADC)
     *   [8..10] = pad
     *   [11] = gas_r MSB
     *   [12] = gas_r LSB[7:6] | gas_valid[5] | range[3:0]
     *
     * Plant ADC values producing plausible readings.
     */
    uint32_t adc_t = 519888;  /* ~25C with above calibration */
    uint32_t adc_p = 415148;  /* ~101325 Pa */
    uint16_t adc_h = 16000;   /* ~50% RH */
    uint16_t adc_gas = 8000;  /* gas ADC */
    uint8_t  gas_range = 4;

    bme_regs[0x1F] = (uint8_t)(adc_p >> 12);
    bme_regs[0x20] = (uint8_t)((adc_p >> 4) & 0xFF);
    bme_regs[0x21] = (uint8_t)((adc_p & 0x0F) << 4);
    bme_regs[0x22] = (uint8_t)(adc_t >> 12);
    bme_regs[0x23] = (uint8_t)((adc_t >> 4) & 0xFF);
    bme_regs[0x24] = (uint8_t)((adc_t & 0x0F) << 4);
    bme_regs[0x25] = (uint8_t)(adc_h >> 8);
    bme_regs[0x26] = (uint8_t)(adc_h & 0xFF);
    /* pad bytes 0x27..0x29 = 0 */
    bme_regs[0x2A] = (uint8_t)(adc_gas >> 2);
    bme_regs[0x2B] = (uint8_t)(((adc_gas & 0x03) << 6) | (1u << 5) | gas_range);
}

static bool bme_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t val)
{
    (void)ctx; (void)dev_addr;
    bme_regs[reg] = val;
    return true;
}

static bool bme_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = bme_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const bme680_i2c_t bme_i2c = {
    .write_reg = bme_write_reg,
    .read_regs = bme_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ======================================================================
 * Mock I2C -- SHT40 (raw write/read, command-based)
 *
 * Init: soft-reset cmd 0x94 (no response), then read-serial cmd 0x89
 *       -> 6 bytes (serial_hi[2] + CRC + serial_lo[2] + CRC)
 * Measure: cmd 0xFD -> 6 bytes (temp[2] + CRC + hum[2] + CRC)
 * ====================================================================== */

static uint8_t sht4_read_buf[6];
static uint8_t sht4_last_cmd;

static void sht4_plant_serial(void)
{
    /* Serial = 0x12345678 -> hi=0x1234, lo=0x5678 */
    uint8_t hi[2] = { 0x12, 0x34 };
    uint8_t lo[2] = { 0x56, 0x78 };
    sht4_read_buf[0] = hi[0];
    sht4_read_buf[1] = hi[1];
    sht4_read_buf[2] = sht40_crc8(hi, 2);
    sht4_read_buf[3] = lo[0];
    sht4_read_buf[4] = lo[1];
    sht4_read_buf[5] = sht40_crc8(lo, 2);
}

static void sht4_plant_reading(float temp_c, float rh_pct)
{
    /*
     * SHT40 conversion:
     *   temp_c = -45 + 175 * raw_t / 65535
     *   rh_pct = -6  + 125 * raw_h / 65535
     */
    uint16_t raw_t = (uint16_t)(((temp_c + 45.0f) / 175.0f) * 65535.0f);
    uint16_t raw_h = (uint16_t)(((rh_pct + 6.0f) / 125.0f) * 65535.0f);

    uint8_t t_bytes[2] = { (uint8_t)(raw_t >> 8), (uint8_t)(raw_t & 0xFF) };
    uint8_t h_bytes[2] = { (uint8_t)(raw_h >> 8), (uint8_t)(raw_h & 0xFF) };

    sht4_read_buf[0] = t_bytes[0];
    sht4_read_buf[1] = t_bytes[1];
    sht4_read_buf[2] = sht40_crc8(t_bytes, 2);
    sht4_read_buf[3] = h_bytes[0];
    sht4_read_buf[4] = h_bytes[1];
    sht4_read_buf[5] = sht40_crc8(h_bytes, 2);
}

static void sht4_mock_reset(void)
{
    memset(sht4_read_buf, 0, sizeof(sht4_read_buf));
    sht4_last_cmd = 0;
    /* Default: serial number response for init */
    sht4_plant_serial();
}

static bool sht4_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                        uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (len >= 1) {
        sht4_last_cmd = data[0];
        /* After serial read consumed, switch to measurement data */
        if (data[0] == 0xFD || data[0] == 0xF6 || data[0] == 0xE0) {
            sht4_plant_reading(25.5f, 52.0f);
        } else if (data[0] == 0x89) {
            /* Read serial command: re-plant serial */
            sht4_plant_serial();
        }
    }
    return true;
}

static bool sht4_read(void *ctx, uint8_t dev_addr, uint8_t *data,
                       uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len && i < sizeof(sht4_read_buf); i++) {
        data[i] = sht4_read_buf[i];
    }
    return true;
}

static const sht40_i2c_t sht4_i2c = {
    .write    = sht4_write,
    .read     = sht4_read,
    .delay_ms = NULL,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock I2C -- AHT20 (raw write/read, command-based)
 *
 * Init: reads 1-byte status (bit[3]=calibrated), optionally sends init cmd
 * Read: trigger cmd 0xAC -> 7 bytes (status + data[5] + CRC)
 * ====================================================================== */

static uint8_t aht_read_buf[7];
static uint8_t aht_last_cmd;
static uint8_t aht_write_count;

static void aht_plant_reading(float temp_c, float rh_pct)
{
    /*
     * AHT20 conversion:
     *   humidity_pct  = raw_h / 2^20 * 100
     *   temperature_c = raw_t / 2^20 * 200 - 50
     *
     * Reverse:
     *   raw_h = rh_pct / 100 * 2^20
     *   raw_t = (temp_c + 50) / 200 * 2^20
     */
    uint32_t raw_h = (uint32_t)((rh_pct / 100.0f) * 1048576.0f);
    uint32_t raw_t = (uint32_t)(((temp_c + 50.0f) / 200.0f) * 1048576.0f);

    /* status: calibrated=1, busy=0 */
    aht_read_buf[0] = (1u << 3);

    /* humidity[19:12], humidity[11:4], (humidity[3:0] << 4) | (temp[19:16]) */
    aht_read_buf[1] = (uint8_t)((raw_h >> 12) & 0xFF);
    aht_read_buf[2] = (uint8_t)((raw_h >> 4) & 0xFF);
    aht_read_buf[3] = (uint8_t)(((raw_h & 0x0F) << 4) |
                                 ((raw_t >> 16) & 0x0F));
    aht_read_buf[4] = (uint8_t)((raw_t >> 8) & 0xFF);
    aht_read_buf[5] = (uint8_t)(raw_t & 0xFF);

    /* CRC over first 6 bytes */
    aht_read_buf[6] = aht20_crc8(aht_read_buf, 6);
}

static void aht_mock_reset(void)
{
    memset(aht_read_buf, 0, sizeof(aht_read_buf));
    aht_last_cmd = 0;
    aht_write_count = 0;
    /* Default: status = calibrated */
    aht_read_buf[0] = (1u << 3);
}

static bool aht_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                       uint16_t len)
{
    (void)ctx; (void)dev_addr;
    aht_write_count++;
    if (len >= 1) {
        aht_last_cmd = data[0];
        /* When trigger measurement is sent, plant data */
        if (data[0] == 0xAC) {
            aht_plant_reading(24.8f, 48.0f);
        }
    }
    return true;
}

static bool aht_read(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len && i < sizeof(aht_read_buf); i++) {
        data[i] = aht_read_buf[i];
    }
    return true;
}

static const aht20_i2c_t aht_i2c = {
    .write    = aht_write,
    .read     = aht_read,
    .delay_ms = NULL,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock I2C -- VEML7700 (register-based: write_regs / read_regs)
 *
 * 16-bit LE registers:
 *   0x00 ALS_CONF
 *   0x04 ALS data
 *   0x05 White data
 * ====================================================================== */

static uint8_t veml_regs[256];

static void veml_plant_lux_raw(uint16_t als_raw, uint16_t white_raw)
{
    /* ALS data at reg 0x04, little-endian */
    veml_regs[0x04 * 2]     = (uint8_t)(als_raw & 0xFF);
    veml_regs[0x04 * 2 + 1] = (uint8_t)(als_raw >> 8);
    /* White data at reg 0x05 */
    veml_regs[0x05 * 2]     = (uint8_t)(white_raw & 0xFF);
    veml_regs[0x05 * 2 + 1] = (uint8_t)(white_raw >> 8);
}

static void veml_mock_reset(void)
{
    memset(veml_regs, 0, sizeof(veml_regs));
    /* Plant ALS raw=1000 (~57.6 lux at gain=1, IT=100ms) */
    veml_plant_lux_raw(1000, 950);
}

static bool veml_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                             const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        veml_regs[reg * 2 + i] = buf[i];
    }
    return true;
}

static bool veml_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = veml_regs[reg * 2 + i];
    }
    return true;
}

static const veml7700_i2c_t veml_i2c = {
    .write_regs = veml_write_regs,
    .read_regs  = veml_read_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static ccs811_t   ccs_dev;
static sgp40_t    sgp_dev;
static bme680_t   bme_dev;
static sht40_t    sht4_dev;
static aht20_t    aht_dev;
static veml7700_t veml_dev;

/* ======================================================================
 * Air Quality Index computation
 *
 * Simple composite AQI from eCO2 and TVOC readings.
 * Based on common indoor air quality breakpoints:
 *   CO2:  400-600 good, 600-1000 moderate, 1000-2000 poor, >2000 unhealthy
 *   TVOC: 0-65 good,   65-220 moderate,    220-660 poor,   >660 unhealthy
 * Returns the worse of the two sub-indices (0-200+).
 * ====================================================================== */

static int compute_aqi(uint16_t eco2_ppm, uint16_t tvoc_ppb)
{
    int co2_aqi;
    if (eco2_ppm <= 600) {
        co2_aqi = (int)((uint32_t)eco2_ppm * (uint32_t)AQI_GOOD / 600u);
    } else if (eco2_ppm <= 1000) {
        co2_aqi = AQI_GOOD + (int)((uint32_t)(eco2_ppm - 600) *
                  (uint32_t)(AQI_MODERATE - AQI_GOOD) / 400u);
    } else if (eco2_ppm <= 2000) {
        co2_aqi = AQI_MODERATE + (int)((uint32_t)(eco2_ppm - 1000) *
                  (uint32_t)(AQI_POOR - AQI_MODERATE) / 1000u);
    } else {
        co2_aqi = AQI_POOR + (int)((uint32_t)(eco2_ppm - 2000) *
                  (uint32_t)(AQI_UNHEALTHY - AQI_POOR) / 2000u);
    }

    int tvoc_aqi;
    if (tvoc_ppb <= 65) {
        tvoc_aqi = (int)((uint32_t)tvoc_ppb * (uint32_t)AQI_GOOD / 65u);
    } else if (tvoc_ppb <= 220) {
        tvoc_aqi = AQI_GOOD + (int)((uint32_t)(tvoc_ppb - 65) *
                   (uint32_t)(AQI_MODERATE - AQI_GOOD) / 155u);
    } else if (tvoc_ppb <= 660) {
        tvoc_aqi = AQI_MODERATE + (int)((uint32_t)(tvoc_ppb - 220) *
                   (uint32_t)(AQI_POOR - AQI_MODERATE) / 440u);
    } else {
        tvoc_aqi = AQI_POOR + (int)((uint32_t)(tvoc_ppb - 660) *
                   (uint32_t)(AQI_UNHEALTHY - AQI_POOR) / 500u);
    }

    return (co2_aqi > tvoc_aqi) ? co2_aqi : tvoc_aqi;
}

static const char *aqi_label(int aqi)
{
    if (aqi <= AQI_GOOD)      return "Good";
    if (aqi <= AQI_MODERATE)  return "Moderate";
    if (aqi <= AQI_POOR)      return "Poor";
    return "Unhealthy";
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. CCS811 */
    ccs_mock_reset();
    ccs811_cfg_t ccs_cfg = {
        .addr = CCS811_ADDR_PRIMARY,
        .mode = CCS811_MODE_1SEC,
    };
    ccs811_err_t ccs_err = ccs811_init(&ccs_dev, &ccs_i2c, &ccs_cfg);
    ASSERT_EQ(ccs_err, CCS811_OK);

    /* 2. SGP40 */
    sgp_mock_reset();
    sgp40_err_t sgp_err = sgp40_init(&sgp_dev, &sgp_i2c);
    ASSERT_EQ(sgp_err, SGP40_OK);

    /* 3. BME680 */
    bme_mock_reset();
    bme680_err_t bme_err = bme680_init(&bme_dev, &bme_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(bme_err, BME680_OK);

    /* 4. SHT40 */
    sht4_mock_reset();
    sht40_err_t sht_err = sht40_init(&sht4_dev, &sht4_i2c, SHT40_ADDR_A);
    ASSERT_EQ(sht_err, SHT40_OK);

    /* 5. AHT20 */
    aht_mock_reset();
    aht20_err_t aht_err = aht20_init(&aht_dev, &aht_i2c);
    ASSERT_EQ(aht_err, AHT20_OK);

    /* 6. VEML7700 */
    veml_mock_reset();
    veml7700_err_t veml_err = veml7700_init(&veml_dev, &veml_i2c,
                                            VEML7700_GAIN_1,
                                            VEML7700_IT_100MS);
    ASSERT_EQ(veml_err, VEML7700_OK);
}

TEST(test_ccs811_read_eco2_tvoc)
{
    ccs811_data_t data;
    ccs811_err_t err = ccs811_read(&ccs_dev, &data);
    ASSERT_EQ(err, CCS811_OK);

    /* Verify planted values: eCO2=650, TVOC=42 */
    ASSERT_EQ(data.eco2_ppm, 650);
    ASSERT_EQ(data.tvoc_ppb, 42);

    printf("\n  CCS811: eCO2=%u ppm, TVOC=%u ppb\n",
           (unsigned)data.eco2_ppm, (unsigned)data.tvoc_ppb);
}

TEST(test_sgp40_read_voc)
{
    sgp40_data_t data;
    sgp40_err_t err = sgp40_measure_raw(&sgp_dev, &data);
    ASSERT_EQ(err, SGP40_OK);

    /* Verify realistic sraw value is in expected range */
    ASSERT_TRUE(data.sraw_voc > 0);
    ASSERT_TRUE(data.sraw_voc < 65535);

    printf("\n  SGP40: SRAW_VOC=%u\n", (unsigned)data.sraw_voc);
}

TEST(test_bme680_read_tphg)
{
    bme680_data_t data;
    bme680_err_t err = bme680_read(&bme_dev, &data);
    ASSERT_EQ(err, BME680_OK);

    /* Temperature should be in plausible range */
    ASSERT_TRUE(data.temperature_c > -40.0f);
    ASSERT_TRUE(data.temperature_c < 85.0f);

    /* Pressure should be plausible */
    ASSERT_TRUE(data.pressure_pa > 30000.0f);
    ASSERT_TRUE(data.pressure_pa < 110000.0f);

    /* Humidity should be 0-100% */
    ASSERT_TRUE(data.humidity_pct >= 0.0f);
    ASSERT_TRUE(data.humidity_pct <= 100.0f);

    /* Gas resistance should be positive (gas_valid=1 in mock) */
    ASSERT_TRUE(data.gas_resistance_ohm > 0.0f);

    printf("\n  BME680: T=%.2f C, P=%.0f Pa, H=%.2f %%, Gas=%.0f ohm\n",
           (double)data.temperature_c, (double)data.pressure_pa,
           (double)data.humidity_pct, (double)data.gas_resistance_ohm);
}

TEST(test_sht40_read)
{
    /* Plant measurement data before reading */
    sht4_plant_reading(25.5f, 52.0f);
    sht40_data_t data;
    sht40_err_t err = sht40_read_high(&sht4_dev, &data);
    ASSERT_EQ(err, SHT40_OK);

    /* Should be close to planted values */
    ASSERT_NEAR(data.temperature_c, 25.5f, 0.5f);
    ASSERT_NEAR(data.humidity_pct, 52.0f, 1.0f);

    printf("\n  SHT40: T=%.2f C, H=%.2f %%\n",
           (double)data.temperature_c, (double)data.humidity_pct);
}

TEST(test_aht20_read)
{
    aht20_data_t data;
    aht20_err_t err = aht20_read(&aht_dev, &data);
    ASSERT_EQ(err, AHT20_OK);

    /* Should be close to planted values (24.8C, 48%) */
    ASSERT_NEAR(data.temperature_c, 24.8f, 0.5f);
    ASSERT_NEAR(data.humidity_pct, 48.0f, 1.0f);

    printf("\n  AHT20: T=%.2f C, H=%.2f %%\n",
           (double)data.temperature_c, (double)data.humidity_pct);
}

TEST(test_veml7700_read_lux)
{
    float lux = 0.0f;
    veml7700_err_t err = veml7700_read_lux(&veml_dev, &lux);
    ASSERT_EQ(err, VEML7700_OK);

    /* At gain=1, IT=100ms, resolution=0.0576: raw=1000 -> 57.6 lux */
    ASSERT_NEAR(lux, 57.6f, 1.0f);
    ASSERT_TRUE(lux > 0.0f);

    printf("\n  VEML7700: lux=%.2f\n", (double)lux);
}

TEST(test_cross_sensor_validation)
{
    /* Read temperature from all three T/H sensors */
    bme680_data_t bme_data;
    bme680_err_t bme_err = bme680_read(&bme_dev, &bme_data);
    ASSERT_EQ(bme_err, BME680_OK);

    sht4_plant_reading(25.5f, 52.0f);
    sht40_data_t sht_data;
    sht40_err_t sht_err = sht40_read_high(&sht4_dev, &sht_data);
    ASSERT_EQ(sht_err, SHT40_OK);

    aht_plant_reading(24.8f, 48.0f);
    aht20_data_t aht_data;
    aht20_err_t aht_err = aht20_read(&aht_dev, &aht_data);
    ASSERT_EQ(aht_err, AHT20_OK);

    printf("\n  --- Cross-sensor Temperature Validation ---\n");
    printf("  BME680: %.2f C\n", (double)bme_data.temperature_c);
    printf("  SHT40:  %.2f C\n", (double)sht_data.temperature_c);
    printf("  AHT20:  %.2f C\n", (double)aht_data.temperature_c);

    /* All three temperatures should be within tolerance of each other */
    float delta_bme_sht = bme_data.temperature_c - sht_data.temperature_c;
    float delta_bme_aht = bme_data.temperature_c - aht_data.temperature_c;
    float delta_sht_aht = sht_data.temperature_c - aht_data.temperature_c;

    if (delta_bme_sht < 0.0f) delta_bme_sht = -delta_bme_sht;
    if (delta_bme_aht < 0.0f) delta_bme_aht = -delta_bme_aht;
    if (delta_sht_aht < 0.0f) delta_sht_aht = -delta_sht_aht;

    printf("  Delta BME680-SHT40: %.2f C\n", (double)delta_bme_sht);
    printf("  Delta BME680-AHT20: %.2f C\n", (double)delta_bme_aht);
    printf("  Delta SHT40-AHT20:  %.2f C\n", (double)delta_sht_aht);

    ASSERT_TRUE(delta_bme_sht < TEMP_CROSS_TOLERANCE);
    ASSERT_TRUE(delta_bme_aht < TEMP_CROSS_TOLERANCE);
    ASSERT_TRUE(delta_sht_aht < TEMP_CROSS_TOLERANCE);

    /* Compute AQI from CCS811 readings */
    ccs811_data_t ccs_data;
    ccs811_err_t ccs_err = ccs811_read(&ccs_dev, &ccs_data);
    ASSERT_EQ(ccs_err, CCS811_OK);

    int aqi = compute_aqi(ccs_data.eco2_ppm, ccs_data.tvoc_ppb);
    ASSERT_TRUE(aqi >= 0);
    ASSERT_TRUE(aqi <= AQI_UNHEALTHY * 2);  /* reasonable upper bound */

    printf("\n  --- Air Quality Index ---\n");
    printf("  eCO2=%u ppm, TVOC=%u ppb -> AQI=%d (%s)\n",
           (unsigned)ccs_data.eco2_ppm, (unsigned)ccs_data.tvoc_ppb,
           aqi, aqi_label(aqi));
}

TEST(test_aqi_computation)
{
    /* Test AQI at known breakpoints */

    /* Good air: CO2=400, TVOC=0 */
    int aqi = compute_aqi(400, 0);
    ASSERT_TRUE(aqi <= AQI_GOOD);

    /* Moderate: CO2=800, TVOC=100 */
    aqi = compute_aqi(800, 100);
    ASSERT_TRUE(aqi > AQI_GOOD);
    ASSERT_TRUE(aqi <= AQI_MODERATE);

    /* Poor: CO2=1500, TVOC=400 */
    aqi = compute_aqi(1500, 400);
    ASSERT_TRUE(aqi > AQI_MODERATE);
    ASSERT_TRUE(aqi <= AQI_POOR);

    /* Unhealthy: CO2=3000, TVOC=800 */
    aqi = compute_aqi(3000, 800);
    ASSERT_TRUE(aqi > AQI_POOR);

    printf("\n  AQI breakpoint checks: all passed\n");
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("air_quality integration")
    RUN(test_init_all);
    RUN(test_ccs811_read_eco2_tvoc);
    RUN(test_sgp40_read_voc);
    RUN(test_bme680_read_tphg);
    RUN(test_sht40_read);
    RUN(test_aht20_read);
    RUN(test_veml7700_read_lux);
    RUN(test_cross_sensor_validation);
    RUN(test_aqi_computation);
TEST_MAIN_END()
