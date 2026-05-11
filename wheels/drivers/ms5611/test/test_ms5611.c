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

/*
 * CRC-4 helper (mirrors ms5611.c internal prom_crc4) so we can compute the
 * correct CRC word for the test PROM and plant it in mock registers.
 */
static uint8_t test_prom_crc4(const uint16_t prom[8])
{
    uint32_t rem = 0;
    uint16_t data[8];
    uint8_t i, bit;
    for (i = 0; i < 8; i++) data[i] = prom[i];
    data[7] = (uint16_t)(data[7] & 0x0FFF); /* zero CRC nibble */
    for (i = 0; i < 16; i++) {
        if (i % 2 == 1) rem ^= (uint32_t)(data[i >> 1] & 0x00FFU);
        else             rem ^= (uint32_t)(data[i >> 1] >> 8);
        for (bit = 8; bit > 0; bit--) {
            if (rem & 0x8000U) rem = (rem << 1) ^ 0x3000U;
            else               rem <<= 1;
        }
    }
    return (uint8_t)((rem >> 12) & 0x000FU);
}

static void plant_prom(void)
{
    /* Datasheet example calibration: word[0]=0, C1-C6, word[7] with CRC */
    uint16_t prom[8] = {
        0x0000,  /* word 0 — factory data, unused */
        40127,   /* C1 = 0x9CBF */
        36924,   /* C2 = 0x903C */
        23317,   /* C3 = 0x5B15 */
        23282,   /* C4 = 0x5AF2 */
        33464,   /* C5 = 0x82B8 */
        28312,   /* C6 = 0x6E98 */
        0x0000,  /* word 7 — CRC in MSN, initialise to 0 */
    };
    uint8_t crc = test_prom_crc4(prom);
    prom[7] = (uint16_t)((uint16_t)crc << 12);

    /* word 0 at 0xA0 */
    mock_regs[0xA0] = (uint8_t)(prom[0] >> 8);
    mock_regs[0xA1] = (uint8_t)(prom[0] & 0xFF);
    /* C1 at 0xA2 */
    mock_regs[0xA2] = (uint8_t)(prom[1] >> 8);
    mock_regs[0xA3] = (uint8_t)(prom[1] & 0xFF);
    /* C2 at 0xA4 */
    mock_regs[0xA4] = (uint8_t)(prom[2] >> 8);
    mock_regs[0xA5] = (uint8_t)(prom[2] & 0xFF);
    /* C3 at 0xA6 */
    mock_regs[0xA6] = (uint8_t)(prom[3] >> 8);
    mock_regs[0xA7] = (uint8_t)(prom[3] & 0xFF);
    /* C4 at 0xA8 */
    mock_regs[0xA8] = (uint8_t)(prom[4] >> 8);
    mock_regs[0xA9] = (uint8_t)(prom[4] & 0xFF);
    /* C5 at 0xAA */
    mock_regs[0xAA] = (uint8_t)(prom[5] >> 8);
    mock_regs[0xAB] = (uint8_t)(prom[5] & 0xFF);
    /* C6 at 0xAC */
    mock_regs[0xAC] = (uint8_t)(prom[6] >> 8);
    mock_regs[0xAD] = (uint8_t)(prom[6] & 0xFF);
    /* word 7 (CRC) at 0xAE */
    mock_regs[0xAE] = (uint8_t)(prom[7] >> 8);
    mock_regs[0xAF] = (uint8_t)(prom[7] & 0xFF);
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

TEST(test_init_crc_fail)
{
    mock_reset();
    /* Corrupt the CRC nibble in word 7 (0xAE byte 0 = MSB).
     * XOR the high byte with 0x10 to flip one CRC bit while
     * keeping lower 12 bits (factory data) intact. */
    mock_regs[0xAE] ^= 0x10;
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_CRC);
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
    RUN(test_init_crc_fail);
TEST_MAIN_END()
