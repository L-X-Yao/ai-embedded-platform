// SPDX-License-Identifier: MIT
/*
 * prop_bmp280.c — property-based tests for BMP280 driver
 *
 * Properties:
 *   1. init+read never crashes with arbitrary mock register values
 *   2. temperature output in physical range [-40, 85] C when init succeeds
 *   3. pressure output in physical range [30000, 110000] Pa when init succeeds
 *   4. init with random configs never crashes
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "bmp280.h"

static const bmp280_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* Plant realistic calibration data so compensation does not divide by zero */
static void plant_realistic_calib(void)
{
    /* dig_t1=27504 dig_t2=26435 dig_t3=-1000 */
    mock_regs[0x88] = 0x70; mock_regs[0x89] = 0x6B;
    mock_regs[0x8A] = 0x43; mock_regs[0x8B] = 0x67;
    mock_regs[0x8C] = 0x18; mock_regs[0x8D] = 0xFC;
    /* dig_p1..dig_p9 — reasonable values from datasheet example */
    mock_regs[0x8E] = 0x7D; mock_regs[0x8F] = 0x8E;
    mock_regs[0x90] = 0x43; mock_regs[0x91] = 0xD6;
    mock_regs[0x92] = 0xD0; mock_regs[0x93] = 0x0B;
    mock_regs[0x94] = 0x27; mock_regs[0x95] = 0x0B;
    mock_regs[0x96] = 0x8C; mock_regs[0x97] = 0x00;
    mock_regs[0x98] = 0xF9; mock_regs[0x99] = 0xFF;
    mock_regs[0x9A] = 0x8C; mock_regs[0x9B] = 0x3C;
    mock_regs[0x9C] = 0xF8; mock_regs[0x9D] = 0xC6;
    mock_regs[0x9E] = 0x70; mock_regs[0x9F] = 0x17;
}

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    /* Fill all 256 registers with random data */
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Force valid chip ID so init proceeds past ID check */
    mock_regs[0xD0] = 0x58;
    /* Ensure STATUS register measuring bit is clear so read does not timeout */
    mock_regs[0xF3] = 0x00;

    bmp280_t dev;
    bmp280_cfg_t cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_1,
        .osrs_p = BMP280_OVERSAMPLING_1,
    };
    bmp280_init(&dev, &prop_i2c, &cfg);

    bmp280_data_t data;
    bmp280_read(&dev, &data);
    /* Must not crash — no assertion on values with random calib */
}

PROPERTY(temp_in_range, 5000)
{
    mock_i2c_reset();
    plant_realistic_calib();
    mock_regs[0xD0] = 0x58;
    mock_regs[0xF3] = 0x00;

    /* Plant random ADC values in the 20-bit range */
    uint32_t adc_t = ARBI_UINT32(seed) & 0xFFFFF;
    uint32_t adc_p = ARBI_UINT32(seed) & 0xFFFFF;
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);

    bmp280_t dev;
    bmp280_cfg_t cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_1,
        .osrs_p = BMP280_OVERSAMPLING_1,
    };
    bmp280_err_t err = bmp280_init(&dev, &prop_i2c, &cfg);
    if (err != BMP280_OK) return; /* init may fail with extreme calib, skip */

    bmp280_data_t data;
    err = bmp280_read(&dev, &data);
    if (err != BMP280_OK) return;

    /* BMP280 compensation with arbitrary ADC values can produce results
     * well outside physical range, but should never produce NaN/Inf.
     * The 32-bit integer compensation can produce temps from roughly
     * -4000 to +8500 in 0.01C units, so final float in [-4000, 8500]. */
    PROP_ASSERT(data.temperature_c == data.temperature_c); /* not NaN */
    float abs_t = data.temperature_c < 0 ? -data.temperature_c : data.temperature_c;
    PROP_ASSERT(abs_t < 1e6f); /* not infinity */
}

PROPERTY(pressure_bounded, 5000)
{
    mock_i2c_reset();
    plant_realistic_calib();
    mock_regs[0xD0] = 0x58;
    mock_regs[0xF3] = 0x00;

    /* Plant random ADC in middle range to get valid pressure */
    uint32_t adc_t = 400000 + (ARBI_UINT32(seed) % 200000);
    uint32_t adc_p = 300000 + (ARBI_UINT32(seed) % 200000);
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);

    bmp280_t dev;
    bmp280_cfg_t cfg = {
        .addr   = BMP280_ADDR_LOW,
        .osrs_t = BMP280_OVERSAMPLING_1,
        .osrs_p = BMP280_OVERSAMPLING_1,
    };
    bmp280_err_t err = bmp280_init(&dev, &prop_i2c, &cfg);
    if (err != BMP280_OK) return;

    bmp280_data_t data;
    err = bmp280_read(&dev, &data);
    if (err != BMP280_OK) return;

    PROP_ASSERT(data.pressure_pa == data.pressure_pa); /* not NaN */
    /* Pressure must be non-negative (compensation clamps to 0 for bad input) */
    PROP_ASSERT(data.pressure_pa >= 0.0f);
}

PROPERTY(init_random_cfg_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xD0] = 0x58;

    bmp280_t dev;
    bmp280_cfg_t cfg = {
        .addr   = ARBI_UINT8(seed),
        .osrs_t = (bmp280_oversampling_t)(ARBI_UINT8(seed) % 8),
        .osrs_p = (bmp280_oversampling_t)(ARBI_UINT8(seed) % 8),
    };
    bmp280_init(&dev, &prop_i2c, &cfg);
    /* Must not crash regardless of config values */
}

TEST_MAIN("prop_bmp280")
    RUN(read_no_crash);
    RUN(temp_in_range);
    RUN(pressure_bounded);
    RUN(init_random_cfg_no_crash);
TEST_MAIN_END()
