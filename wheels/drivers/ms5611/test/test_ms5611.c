// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ms5611.h"
#include <math.h>

/*
 * MS5611 uses command-based I2C. We abuse mock_regs:
 * - write_reg(addr, cmd, 0) stores cmd to trigger behavior
 * - read_regs(addr, 0x00, buf, 3) reads ADC data from mock_regs[0..2]
 * - read_regs(addr, 0xA2..0xAC, buf, 2) reads PROM from mock_regs[prom_addr..+1]
 *
 * Plant PROM calibration values from the MS5611 datasheet example:
 *   C1=40127, C2=36924, C3=23317, C4=23282, C5=33464, C6=28312
 */

static void plant_prom(void)
{
    /* C1 at 0xA2 */
    mock_regs[0xA2] = 0x9C; mock_regs[0xA3] = 0xBF; /* 40127 = 0x9CBF */
    /* C2 at 0xA4 */
    mock_regs[0xA4] = 0x90; mock_regs[0xA5] = 0x3C; /* 36924 = 0x903C */
    /* C3 at 0xA6 */
    mock_regs[0xA6] = 0x5B; mock_regs[0xA7] = 0x15; /* 23317 = 0x5B15 */
    /* C4 at 0xA8 */
    mock_regs[0xA8] = 0x5A; mock_regs[0xA9] = 0xF2; /* 23282 = 0x5AF2 */
    /* C5 at 0xAA */
    mock_regs[0xAA] = 0x82; mock_regs[0xAB] = 0xB8; /* 33464 = 0x82B8 */
    /* C6 at 0xAC */
    mock_regs[0xAC] = 0x6E; mock_regs[0xAD] = 0x98; /* 28312 = 0x6E98 */
}

static void plant_adc(uint32_t val)
{
    mock_regs[0x00] = (uint8_t)(val >> 16);
    mock_regs[0x01] = (uint8_t)(val >> 8);
    mock_regs[0x02] = (uint8_t)(val);
}

static void mock_reset(void)
{
    mock_i2c_reset();
    plant_prom();
}

static const ms5611_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static ms5611_t dev;
static ms5611_cfg_t default_cfg = {
    .addr = MS5611_ADDR_CSB_LOW,
    .osr  = MS5611_OSR_4096,
};

static void setup(void)
{
    mock_reset();
    ms5611_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* Reset command fails */
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ms5611_init(NULL, &mock_i2c, &default_cfg), MS5611_ERR_PARAM);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, NULL, &default_cfg), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_init(&d, &mock_i2c, NULL), MS5611_ERR_PARAM);
    ms5611_data_t data;
    ASSERT_EQ(ms5611_read(NULL, &data), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_read(&d, NULL), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_reset(NULL), MS5611_ERR_PARAM);
}

TEST(test_prom_readback)
{
    setup();
    ASSERT_EQ(dev.calib.c1, 40127);
    ASSERT_EQ(dev.calib.c2, 36924);
    ASSERT_EQ(dev.calib.c3, 23317);
    ASSERT_EQ(dev.calib.c4, 23282);
    ASSERT_EQ(dev.calib.c5, 33464);
    ASSERT_EQ(dev.calib.c6, 28312);
}

TEST(test_read_ok)
{
    setup();

    /*
     * With register-based mock, both D1 and D2 read the same ADC value.
     * Plant a value that produces valid (non-NaN) output with the example
     * calibration data. D1=D2=8569150 (0x82BE9E).
     */
    plant_adc(8569150);

    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* Verify outputs are finite (not NaN) */
    ASSERT_TRUE(data.temperature_c == data.temperature_c);
    ASSERT_TRUE(data.pressure_pa == data.pressure_pa);
}

TEST(test_read_compensation)
{
    setup();

    /*
     * Verify compensation produces plausible temperature from known ADC.
     * D2 = C5*256 + 256 => dT = 256 => temp ~ 2000 + tiny = ~20C
     * D1 = same value (mock limitation).
     * ADC = 33464*256 + 256 = 8567040.
     */
    plant_adc(8567040);

    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* Temperature near 20C (dT is small => temp ~ 2000/100 = 20) */
    ASSERT_TRUE(data.temperature_c > 19.0f && data.temperature_c < 21.0f);
}

TEST(test_read_i2c_fail_d1)
{
    setup();
    /* D1 convert command fails (first op after init) */
    mock_fail_next = 1;
    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    ms5611_err_t e = ms5611_reset(&dev);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ms5611_err_t e = ms5611_reset(&dev);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_addr_csb_high)
{
    mock_reset();
    ms5611_cfg_t cfg = {
        .addr = MS5611_ADDR_CSB_HIGH,
        .osr  = MS5611_OSR_4096,
    };
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_prom_read_i2c_fail)
{
    mock_reset();
    /* Reset succeeds (op 0), first PROM read fails (op 1) */
    mock_fail_at = 1;
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_osr_invalid)
{
    mock_reset();
    ms5611_cfg_t cfg = {
        .addr = MS5611_ADDR_CSB_LOW,
        .osr  = (ms5611_osr_t)99,
    };
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, MS5611_ERR_PARAM);
}

TEST(test_delay_called)
{
    setup();
    uint32_t before = mock_delay_ms_count;
    plant_adc(9085466);
    ms5611_data_t data;
    ms5611_read(&dev, &data);
    /* At least 2 delays: one for D1, one for D2 */
    ASSERT_TRUE(mock_delay_ms_count > before + 1);
}

TEST_MAIN("ms5611")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_prom_readback);
    RUN(test_read_ok);
    RUN(test_read_compensation);
    RUN(test_read_i2c_fail_d1);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_csb_high);
    RUN(test_prom_read_i2c_fail);
    RUN(test_osr_invalid);
    RUN(test_delay_called);
TEST_MAIN_END()
