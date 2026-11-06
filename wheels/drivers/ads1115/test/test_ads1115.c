// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ads1115.h"
#include <string.h>
#include <math.h>

/*
 * Mock I2C. Like the device, it clears OS when a conversion starts and
 * shows the new result only when OS is back at 1: p.18 "resets the OS bit
 * to 0b, and starts a single conversion", p.24 "the result of the last
 * conversion".
 */
#define MOCK_BUSY_POLLS 3

static uint8_t mock_config_written[2];
static int16_t mock_conversion_value;
static int16_t mock_result_register;
static int mock_busy_left;
static int mock_config_reads;
static bool mock_i2c_fail;

static bool mock_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)val;
    if (mock_i2c_fail) return false;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    if (reg == 0x00 && len == 2) {
        data[0] = (uint8_t)((uint16_t)mock_result_register >> 8);
        data[1] = (uint8_t)(mock_result_register & 0xFF);
    } else if (reg == 0x01 && len == 2) {
        mock_config_reads++;
        if (mock_busy_left > 0) {
            mock_busy_left--;
            data[0] = mock_config_written[0] & 0x7F;
        } else {
            mock_result_register = mock_conversion_value;
            data[0] = 0x80 | mock_config_written[0];
        }
        data[1] = mock_config_written[1];
    }
    return true;
}

static bool mock_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    if (reg == 0x01 && len == 2) {
        mock_config_written[0] = data[0];
        mock_config_written[1] = data[1];
        if (data[0] & 0x80)
            mock_busy_left = MOCK_BUSY_POLLS;
    }
    return true;
}

static ads1115_i2c_t mock_i2c = {
    .write_reg  = mock_write_reg,
    .read_regs  = mock_read_regs,
    .write_regs = mock_write_regs,
    .ctx        = NULL
};

TEST(test_init_null)
{
    ads1115_cfg_t cfg = { .addr = ADS1115_ADDR_GND };
    ASSERT_EQ(ads1115_init(NULL, &mock_i2c, &cfg), ADS1115_ERR_PARAM);
    ads1115_t dev;
    ASSERT_EQ(ads1115_init(&dev, NULL, &cfg), ADS1115_ERR_PARAM);
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, NULL), ADS1115_ERR_PARAM);
}

TEST(test_read_raw_positive)
{
    mock_i2c_fail = false;
    mock_conversion_value = 16384;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_4096,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    ASSERT_EQ(raw, 16384);
}

TEST(test_read_raw_negative)
{
    mock_i2c_fail = false;
    mock_conversion_value = -8192;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_AIN1,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    ASSERT_EQ(raw, -8192);
}

TEST(test_read_waits_for_end_of_conversion)
{
    mock_i2c_fail = false;
    mock_result_register = 111;       /* last result, stale */
    mock_conversion_value = 222;      /* result of the new conversion */
    mock_config_reads = 0;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    ASSERT_EQ(raw, 222);
    ASSERT_EQ(mock_config_reads, MOCK_BUSY_POLLS + 1);
}

TEST(test_read_voltage)
{
    mock_i2c_fail = false;
    mock_conversion_value = 32767;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_4096,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    float mv;
    ASSERT_EQ(ads1115_read_voltage(&dev, &mv), ADS1115_OK);
    ASSERT_NEAR(mv, 4096.0f, 1.0f);
}

TEST(test_read_voltage_half_scale)
{
    mock_i2c_fail = false;
    mock_conversion_value = 16384;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    float mv;
    ASSERT_EQ(ads1115_read_voltage(&dev, &mv), ADS1115_OK);
    ASSERT_NEAR(mv, 1024.0f, 1.0f);
}

TEST(test_i2c_failure)
{
    mock_i2c_fail = true;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_ERR_I2C);
}

TEST(test_set_channel)
{
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    ASSERT_EQ(ads1115_set_channel(&dev, ADS1115_MUX_AIN1_GND), ADS1115_OK);
    ASSERT_EQ(ads1115_set_channel(NULL, ADS1115_MUX_AIN1_GND), ADS1115_ERR_PARAM);
}

TEST(test_config_bits)
{
    mock_i2c_fail = false;
    mock_conversion_value = 0;
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ASSERT_EQ(ads1115_init(&dev, &mock_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ads1115_read_raw(&dev, &raw);
    uint16_t written = (uint16_t)(((uint16_t)mock_config_written[0] << 8) | mock_config_written[1]);
    ASSERT_EQ((written >> 15) & 1, 1);
    ASSERT_EQ((written >> 12) & 7, ADS1115_MUX_AIN0_GND);
    ASSERT_EQ((written >> 9) & 7, ADS1115_PGA_2048);
    ASSERT_EQ((written >> 8) & 1, 1);
}

/* Mock that never reports conversion complete (bit 15 stays clear) */
static bool mock_busy_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (reg == 0x01 && len == 2) {
        /* Config register: bit 15 always 0 = conversion not ready */
        data[0] = 0x00;
        data[1] = 0x00;
    } else if (reg == 0x00 && len == 2) {
        data[0] = 0;
        data[1] = 0;
    }
    return true;
}

TEST(test_read_busy_timeout)
{
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128
    };
    ads1115_i2c_t busy_i2c = {
        .write_reg  = mock_write_reg,
        .read_regs  = mock_busy_read_regs,
        .write_regs = mock_write_regs,
        .ctx        = NULL
    };
    ASSERT_EQ(ads1115_init(&dev, &busy_i2c, &cfg), ADS1115_OK);
    int16_t raw;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_ERR_BUSY);
}

TEST_MAIN("ads1115")
    RUN(test_init_null);
    RUN(test_read_raw_positive);
    RUN(test_read_raw_negative);
    RUN(test_read_waits_for_end_of_conversion);
    RUN(test_read_voltage);
    RUN(test_read_voltage_half_scale);
    RUN(test_i2c_failure);
    RUN(test_set_channel);
    RUN(test_config_bits);
    RUN(test_read_busy_timeout);
TEST_MAIN_END()
