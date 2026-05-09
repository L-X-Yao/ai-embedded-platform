// SPDX-License-Identifier: MIT
/*
 * prop_lps22hb.c — property-based tests for lps22hb
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_lps22hb.c ../lps22hb.c -o prop_lps22hb
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "lps22hb.h"

/* Register addresses */
#define REG_WHO_AM_I  0x0F
#define REG_CTRL_REG1 0x10
#define REG_PRESS_XL  0x28
#define REG_PRESS_L   0x29
#define REG_PRESS_H   0x2A
#define REG_TEMP_L    0x2B
#define REG_TEMP_H    0x2C

static const lps22hb_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void mock_set_pressure_raw(int32_t raw)
{
    mock_regs[REG_PRESS_XL] = (uint8_t)(raw & 0xFF);
    mock_regs[REG_PRESS_L]  = (uint8_t)((raw >> 8) & 0xFF);
    mock_regs[REG_PRESS_H]  = (uint8_t)((raw >> 16) & 0xFF);
}

static void mock_set_temperature_raw(int16_t raw)
{
    mock_regs[REG_TEMP_L] = (uint8_t)(raw & 0xFF);
    mock_regs[REG_TEMP_H] = (uint8_t)((raw >> 8) & 0xFF);
}

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_err_t err = lps22hb_init(&dev, &prop_i2c);
    if (err != LPS22HB_OK) return;

    lps22hb_data_t data;
    lps22hb_read(&dev, &data);
    /* Must not crash with arbitrary register values */
}

PROPERTY(read_output_finite, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_err_t err = lps22hb_init(&dev, &prop_i2c);
    if (err != LPS22HB_OK) return;

    /* Plant random 24-bit pressure and 16-bit temperature */
    int32_t raw_p = (int32_t)(ARBI_UINT32(seed) & 0xFFFFFF);
    if (raw_p & 0x800000) raw_p |= (int32_t)0xFF000000; /* sign extend */
    int16_t raw_t = (int16_t)ARBI_UINT16(seed);

    mock_set_pressure_raw(raw_p);
    mock_set_temperature_raw(raw_t);

    lps22hb_data_t data;
    err = lps22hb_read(&dev, &data);
    if (err != LPS22HB_OK) return;

    /* Outputs must not be NaN */
    PROP_ASSERT(data.pressure_hpa == data.pressure_hpa);
    PROP_ASSERT(data.temperature_c == data.temperature_c);
}

PROPERTY(realistic_pressure_in_range, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_err_t err = lps22hb_init(&dev, &prop_i2c);
    if (err != LPS22HB_OK) return;

    /* Generate realistic pressure: 260-1260 hPa range from datasheet */
    float target_hpa = 260.0f + (float)(ARBI_UINT32(seed) % 10000) / 10.0f;
    int32_t raw_p = (int32_t)(target_hpa * 4096.0f);
    mock_set_pressure_raw(raw_p);
    mock_set_temperature_raw(2500);  /* 25.0 C */

    lps22hb_data_t data;
    err = lps22hb_read(&dev, &data);
    if (err != LPS22HB_OK) return;

    PROP_ASSERT(data.pressure_hpa >= 250.0f);
    PROP_ASSERT(data.pressure_hpa <= 1270.0f);
}

PROPERTY(odr_variants, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_err_t err = lps22hb_init(&dev, &prop_i2c);
    if (err != LPS22HB_OK) return;

    lps22hb_odr_t odr = (lps22hb_odr_t)(ARBI_UINT8(seed) % 6);
    err = lps22hb_set_odr(&dev, odr);
    PROP_ASSERT(err == LPS22HB_OK);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_init(&dev, &prop_i2c);
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;
    lps22hb_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(raw_pressure_roundtrip, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = LPS22HB_CHIP_ID;

    lps22hb_t dev;
    lps22hb_err_t err = lps22hb_init(&dev, &prop_i2c);
    if (err != LPS22HB_OK) return;

    /* Plant a positive 24-bit value */
    int32_t planted = (int32_t)(ARBI_UINT32(seed) & 0x7FFFFF);
    mock_set_pressure_raw(planted);

    int32_t raw = 0;
    err = lps22hb_read_raw_pressure(&dev, &raw);
    if (err != LPS22HB_OK) return;

    PROP_ASSERT_EQ(raw, planted);
}

TEST_MAIN("lps22hb properties")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(read_output_finite);
    RUN(realistic_pressure_in_range);
    RUN(odr_variants);
    RUN(double_init_safe);
    RUN(raw_pressure_roundtrip);
TEST_MAIN_END()
