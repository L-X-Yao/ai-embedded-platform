// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ms5611.h"

/*
 * MS5611 mock: command-based protocol.
 * write_reg(addr, cmd, 0) → sends command, stored in mock_regs[cmd]
 * read_regs(addr, 0, buf, len) → reads from "adc result" or "prom" area
 *
 * We track last command sent to determine what data to return on read.
 */

static uint8_t last_cmd;
static uint16_t prom_data[8];
static uint32_t adc_result;

static void mock_reset(void)
{
    mock_i2c_reset();
    last_cmd = 0;
    adc_result = 0;
    memset(prom_data, 0, sizeof(prom_data));

    /* Example calibration from MS5611 datasheet (AN520 application note) */
    prom_data[0] = 0x0000;   /* reserved / manufacturer */
    prom_data[1] = 40127;    /* C1 - pressure sensitivity */
    prom_data[2] = 36924;    /* C2 - pressure offset */
    prom_data[3] = 23317;    /* C3 - temp coef of press sensitivity */
    prom_data[4] = 23282;    /* C4 - temp coef of press offset */
    prom_data[5] = 33464;    /* C5 - reference temperature */
    prom_data[6] = 28312;    /* C6 - temp coef of temperature */
    prom_data[7] = 0x0000;   /* CRC */
}

/* Custom write_reg that tracks commands */
static bool mock_cmd_write(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)val;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    last_cmd = reg;
    return true;
}

/* Custom read_regs that returns data based on last command */
static bool mock_cmd_read(void *ctx, uint8_t dev_addr,
                          uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;

    /* PROM read: commands 0xA0..0xAE */
    if (last_cmd >= 0xA0 && last_cmd <= 0xAE) {
        uint8_t idx = (uint8_t)((last_cmd - 0xA0) / 2);
        if (len >= 2) {
            buf[0] = (uint8_t)(prom_data[idx] >> 8);
            buf[1] = (uint8_t)(prom_data[idx] & 0xFF);
        }
        return true;
    }

    /* ADC read: command 0x00 */
    if (last_cmd == 0x00 && len >= 3) {
        buf[0] = (uint8_t)(adc_result >> 16);
        buf[1] = (uint8_t)(adc_result >> 8);
        buf[2] = (uint8_t)(adc_result & 0xFF);
        return true;
    }

    /* Other reads: zero fill */
    memset(buf, 0, len);
    return true;
}

static const ms5611_i2c_t mock_i2c_cmd = {
    .write_reg = mock_cmd_write,
    .read_regs = mock_cmd_read,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static ms5611_t dev;
static ms5611_cfg_t default_cfg = {
    .addr = MS5611_DEV_ADDR,
    .osr  = MS5611_OSR_4096,
};

/* Track which conversion is being requested to return different ADC values */
static uint32_t d1_pressure;
static uint32_t d2_temperature;
static int      conversion_num;

static bool mock_cmd_write_track(void *ctx, uint8_t dev_addr,
                                 uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)val;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    last_cmd = reg;

    /* When ADC conversion command is sent, prepare the right result */
    if (reg >= 0x40 && reg <= 0x48) {
        /* D1 pressure conversion */
        adc_result = d1_pressure;
    } else if (reg >= 0x50 && reg <= 0x58) {
        /* D2 temperature conversion */
        adc_result = d2_temperature;
    }
    return true;
}

static const ms5611_i2c_t mock_i2c_track = {
    .write_reg = mock_cmd_write_track,
    .read_regs = mock_cmd_read,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void setup(void)
{
    mock_reset();
    conversion_num = 0;
    /* Datasheet example: D1=9085466, D2=8569150 → 20.07°C, 100009 Pa */
    d1_pressure    = 9085466;
    d2_temperature = 8569150;
    ms5611_init(&dev, &mock_i2c_track, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c_cmd, &default_cfg);
    ASSERT_EQ(e, MS5611_OK);
}

TEST(test_init_reads_prom)
{
    mock_reset();
    ms5611_t d;
    ms5611_init(&d, &mock_i2c_cmd, &default_cfg);
    ASSERT_EQ(d.prom[1], 40127);
    ASSERT_EQ(d.prom[5], 33464);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ms5611_t d;
    ms5611_err_t e = ms5611_init(&d, &mock_i2c_cmd, &default_cfg);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ms5611_init(NULL, &mock_i2c_cmd, &default_cfg), MS5611_ERR_PARAM);
    ms5611_t d;
    ASSERT_EQ(ms5611_init(&d, NULL, &default_cfg), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_init(&d, &mock_i2c_cmd, NULL), MS5611_ERR_PARAM);
    ms5611_data_t data;
    ASSERT_EQ(ms5611_read(NULL, &data), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_read(&d, NULL), MS5611_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    ms5611_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ms5611_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ms5611_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)MS5611_ERR_I2C);
    ASSERT_EQ(-2, (int)MS5611_ERR_PROM);
    ASSERT_EQ(-3, (int)MS5611_ERR_PARAM);
}

TEST(test_read_temperature)
{
    setup();
    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* Datasheet example: ~20°C */
    ASSERT_TRUE(data.temperature_c > 15.0f && data.temperature_c < 25.0f);
}

TEST(test_read_pressure)
{
    setup();
    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_OK);
    /* ~100009 Pa (about 1 atm) */
    ASSERT_TRUE(data.pressure_pa > 90000.0f && data.pressure_pa < 110000.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ms5611_data_t data;
    ms5611_err_t e = ms5611_read(&dev, &data);
    ASSERT_EQ(e, MS5611_ERR_I2C);
}

TEST(test_invalid_osr)
{
    mock_reset();
    ms5611_t d;
    ms5611_cfg_t cfg = { .addr = MS5611_DEV_ADDR, .osr = (ms5611_osr_t)5 };
    ms5611_err_t e = ms5611_init(&d, &mock_i2c_cmd, &cfg);
    ASSERT_EQ(e, MS5611_ERR_PARAM);
}

TEST(test_delay_called)
{
    mock_reset();
    mock_delay_ms_count = 0;
    ms5611_t d;
    ms5611_init(&d, &mock_i2c_cmd, &default_cfg);
    /* At least one delay for reset */
    ASSERT_TRUE(mock_delay_ms_count > 0);
}

TEST_MAIN("ms5611")
    RUN(test_init_ok);
    RUN(test_init_reads_prom);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_temperature);
    RUN(test_read_pressure);
    RUN(test_read_i2c_fail);
    RUN(test_invalid_osr);
    RUN(test_delay_called);
TEST_MAIN_END()
