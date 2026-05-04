// SPDX-License-Identifier: MIT
/*
 * prop_bme280.c — property-based tests for BME280 driver
 *
 * Properties:
 *   1. init+read never crashes with arbitrary mock register values
 *   2. temperature output bounded with realistic calibration
 *   3. humidity output in [0, 100]% with realistic calibration
 *   4. random oversampling config never crashes
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "bme280.h"

static const bme280_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void plant_realistic_calib(void)
{
    /* Temperature + pressure calibration (same as bmp280 example) */
    mock_regs[0x88] = 0x70; mock_regs[0x89] = 0x6B;
    mock_regs[0x8A] = 0x43; mock_regs[0x8B] = 0x67;
    mock_regs[0x8C] = 0x18; mock_regs[0x8D] = 0xFC;
    mock_regs[0x8E] = 0x7D; mock_regs[0x8F] = 0x8E;
    mock_regs[0x90] = 0x43; mock_regs[0x91] = 0xD6;
    mock_regs[0x92] = 0xD0; mock_regs[0x93] = 0x0B;
    mock_regs[0x94] = 0x27; mock_regs[0x95] = 0x0B;
    mock_regs[0x96] = 0x8C; mock_regs[0x97] = 0x00;
    mock_regs[0x98] = 0xF9; mock_regs[0x99] = 0xFF;
    mock_regs[0x9A] = 0x8C; mock_regs[0x9B] = 0x3C;
    mock_regs[0x9C] = 0xF8; mock_regs[0x9D] = 0xC6;
    mock_regs[0x9E] = 0x70; mock_regs[0x9F] = 0x17;
    /* dig_h1 */
    mock_regs[0xA1] = 0x4B;
    /* Humidity calibration (0xE1..0xE7) */
    mock_regs[0xE1] = 0x6E; mock_regs[0xE2] = 0x01;
    mock_regs[0xE3] = 0x00;
    mock_regs[0xE4] = 0x13; mock_regs[0xE5] = 0x62;
    mock_regs[0xE6] = 0x00;
    mock_regs[0xE7] = 0x1E;
}

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xD0] = 0x60; /* BME280 chip ID */
    mock_regs[0xF3] = 0x00; /* STATUS: not measuring */

    bme280_t dev;
    bme280_cfg_t cfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = BME280_OVERSAMPLING_1,
        .osrs_p = BME280_OVERSAMPLING_1,
        .osrs_h = BME280_OVERSAMPLING_1,
    };
    bme280_init(&dev, &prop_i2c, &cfg);

    bme280_data_t data;
    bme280_read(&dev, &data);
    /* Must not crash */
}

PROPERTY(temp_bounded, 5000)
{
    mock_i2c_reset();
    plant_realistic_calib();
    mock_regs[0xD0] = 0x60;
    mock_regs[0xF3] = 0x00;

    uint32_t adc_t = ARBI_UINT32(seed) & 0xFFFFF;
    uint32_t adc_p = ARBI_UINT32(seed) & 0xFFFFF;
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
    /* Humidity ADC (0xFD-0xFE) */
    mock_regs[0xFD] = ARBI_UINT8(seed);
    mock_regs[0xFE] = ARBI_UINT8(seed);

    bme280_t dev;
    bme280_cfg_t cfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = BME280_OVERSAMPLING_1,
        .osrs_p = BME280_OVERSAMPLING_1,
        .osrs_h = BME280_OVERSAMPLING_1,
    };
    bme280_err_t err = bme280_init(&dev, &prop_i2c, &cfg);
    if (err != BME280_OK) return;

    bme280_data_t data;
    err = bme280_read(&dev, &data);
    if (err != BME280_OK) return;

    /* BME280 compensation with arbitrary ADC values can produce results
     * well outside physical range, but should never produce NaN/Inf. */
    PROP_ASSERT(data.temperature_c == data.temperature_c); /* not NaN */
    float abs_t = data.temperature_c < 0 ? -data.temperature_c : data.temperature_c;
    PROP_ASSERT(abs_t < 1e6f); /* not infinity */
}

PROPERTY(humidity_bounded, 5000)
{
    mock_i2c_reset();
    plant_realistic_calib();
    mock_regs[0xD0] = 0x60;
    mock_regs[0xF3] = 0x00;

    /* Plant mid-range ADC for stable temperature (needed for humidity calc) */
    uint32_t adc_t = 400000 + (ARBI_UINT32(seed) % 200000);
    uint32_t adc_p = 400000 + (ARBI_UINT32(seed) % 200000);
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
    mock_regs[0xFD] = ARBI_UINT8(seed);
    mock_regs[0xFE] = ARBI_UINT8(seed);

    bme280_t dev;
    bme280_cfg_t cfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = BME280_OVERSAMPLING_1,
        .osrs_p = BME280_OVERSAMPLING_1,
        .osrs_h = BME280_OVERSAMPLING_1,
    };
    bme280_err_t err = bme280_init(&dev, &prop_i2c, &cfg);
    if (err != BME280_OK) return;

    bme280_data_t data;
    err = bme280_read(&dev, &data);
    if (err != BME280_OK) return;

    PROP_ASSERT(data.humidity_pct == data.humidity_pct); /* not NaN */
    /* BME280 compensation clamps humidity to [0, 100] */
    PROP_ASSERT(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);
}

PROPERTY(random_oversampling_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xD0] = 0x60;

    bme280_t dev;
    bme280_cfg_t cfg = {
        .addr   = BME280_ADDR_LOW,
        .osrs_t = (bme280_oversampling_t)(ARBI_UINT8(seed) % 8),
        .osrs_p = (bme280_oversampling_t)(ARBI_UINT8(seed) % 8),
        .osrs_h = (bme280_oversampling_t)(ARBI_UINT8(seed) % 8),
    };
    bme280_init(&dev, &prop_i2c, &cfg);
    /* Must not crash regardless of config values */
}

TEST_MAIN("prop_bme280")
    RUN(read_no_crash);
    RUN(temp_bounded);
    RUN(humidity_bounded);
    RUN(random_oversampling_no_crash);
TEST_MAIN_END()
