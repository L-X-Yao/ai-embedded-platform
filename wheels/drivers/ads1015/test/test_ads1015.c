// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ads1015.h"
#include <string.h>
#include <math.h>

/* ---- custom mock for 16-bit register-pair device ---- */

/*
 * The ADS1015 has two 16-bit registers accessed by pointer-byte:
 *   0x00 = conversion (read-only), 0x01 = config (r/w).
 * Each is 2 bytes wide. The shared mock_i2c.h uses a flat byte array
 * which causes overlap (conv[1] == config[0]). We use a dedicated mock.
 */

static uint8_t  mock_config[2];       /* last config written */
static int16_t  mock_conv_12bit;      /* 12-bit value to return */
static bool     mock_ready;           /* conversion-complete flag */
static bool     mock_i2c_fail;

static void mock_reset(void)
{
    mock_config[0] = 0x85u;
    mock_config[1] = 0x83u;
    mock_conv_12bit = 0;
    mock_ready = true;
    mock_i2c_fail = false;
}

static bool mock_write_reg(void *ctx, uint8_t dev, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev; (void)reg; (void)val;
    return !mock_i2c_fail;
}

static bool mock_read_regs(void *ctx, uint8_t dev, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev;
    if (mock_i2c_fail) return false;
    if (reg == 0x00u && len == 2u) {
        /* Conversion register: 12-bit left-justified in 16 bits */
        int16_t lj = (int16_t)((uint16_t)mock_conv_12bit << 4);
        buf[0] = (uint8_t)((uint16_t)lj >> 8);
        buf[1] = (uint8_t)((uint16_t)lj & 0xFFu);
    } else if (reg == 0x01u && len == 2u) {
        /* Config register: inject ready bit */
        buf[0] = mock_ready ? (mock_config[0] | 0x80u) : (uint8_t)(mock_config[0] & 0x7Fu);
        buf[1] = mock_config[1];
    }
    return true;
}

static bool mock_write_regs(void *ctx, uint8_t dev, uint8_t reg,
                            const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev;
    if (mock_i2c_fail) return false;
    if (reg == 0x01u && len == 2u) {
        mock_config[0] = buf[0];
        mock_config[1] = buf[1];
    }
    return true;
}

static const ads1015_i2c_t mock_i2c = {
    .write_reg  = mock_write_reg,
    .read_regs  = mock_read_regs,
    .write_regs = mock_write_regs,
    .ctx        = NULL,
};

static ads1015_cfg_t default_cfg(void)
{
    ads1015_cfg_t cfg;
    cfg.addr = ADS1015_ADDR_GND;
    cfg.pga  = ADS1015_PGA_2048;
    cfg.rate = ADS1015_RATE_1600;
    return cfg;
}

/* ---- tests ---- */

TEST(test_init_ok)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
}

TEST(test_init_null_dev)
{
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(NULL, &mock_i2c, &cfg), ADS1015_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, NULL, &cfg), ADS1015_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    ads1015_t dev;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, NULL), ADS1015_ERR_PARAM);
}

TEST(test_init_bad_pga)
{
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.pga = (ads1015_pga_t)6;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_ERR_PARAM);
}

TEST(test_init_bad_rate)
{
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.rate = (ads1015_rate_t)7;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_ERR_PARAM);
}

TEST(test_read_raw_positive)
{
    mock_reset();
    mock_conv_12bit = 1024;  /* half-scale positive */
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw), ADS1015_OK);
    ASSERT_EQ(raw, 1024);
}

TEST(test_read_raw_negative)
{
    mock_reset();
    mock_conv_12bit = -512;
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_AIN1, &raw), ADS1015_OK);
    ASSERT_EQ(raw, -512);
}

TEST(test_read_raw_full_scale)
{
    mock_reset();
    mock_conv_12bit = 2047;  /* max positive 12-bit */
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw), ADS1015_OK);
    ASSERT_EQ(raw, 2047);
}

TEST(test_read_single_voltage)
{
    mock_reset();
    /* PGA_4096: scale = 4096/2048 = 2.0 mV/LSB.  raw=1024 => 2048 mV */
    mock_conv_12bit = 1024;
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.pga = ADS1015_PGA_4096;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    float mv;
    ASSERT_EQ(ads1015_read_single(&dev, ADS1015_MUX_AIN0_GND, &mv), ADS1015_OK);
    ASSERT_NEAR(mv, 2048.0f, 1.0f);
}

TEST(test_read_single_full_scale)
{
    mock_reset();
    /* PGA_6144: scale = 6144/2048 = 3.0 mV/LSB.  raw=2047 => 6141 mV */
    mock_conv_12bit = 2047;
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.pga = ADS1015_PGA_6144;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    float mv;
    ASSERT_EQ(ads1015_read_single(&dev, ADS1015_MUX_AIN0_GND, &mv), ADS1015_OK);
    ASSERT_NEAR(mv, 6141.0f, 1.0f);
}

TEST(test_i2c_write_failure)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    mock_i2c_fail = true;
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw),
              ADS1015_ERR_I2C);
}

TEST(test_busy_timeout)
{
    mock_reset();
    mock_ready = false;  /* poll never sees conversion complete */
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw),
              ADS1015_ERR_BUSY);
}

TEST(test_set_pga)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    ASSERT_EQ(ads1015_set_pga(&dev, ADS1015_PGA_0256), ADS1015_OK);
    ASSERT_EQ(ads1015_set_pga(NULL, ADS1015_PGA_0256), ADS1015_ERR_PARAM);
    ASSERT_EQ(ads1015_set_pga(&dev, (ads1015_pga_t)6), ADS1015_ERR_PARAM);
}

TEST(test_set_rate)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    ASSERT_EQ(ads1015_set_rate(&dev, ADS1015_RATE_3300), ADS1015_OK);
    ASSERT_EQ(ads1015_set_rate(NULL, ADS1015_RATE_3300), ADS1015_ERR_PARAM);
    ASSERT_EQ(ads1015_set_rate(&dev, (ads1015_rate_t)7), ADS1015_ERR_PARAM);
}

TEST(test_config_bits)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.pga  = ADS1015_PGA_4096;
    cfg.rate = ADS1015_RATE_920;
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN2_GND, &raw);
    uint16_t written = (uint16_t)(((uint16_t)mock_config[0] << 8) | mock_config[1]);
    /* bit 15: OS = 1 (start) */
    ASSERT_EQ((written >> 15) & 1u, 1u);
    /* bits 14:12: MUX = AIN2_GND = 6 */
    ASSERT_EQ((written >> 12) & 7u, (unsigned)ADS1015_MUX_AIN2_GND);
    /* bits 11:9: PGA = PGA_4096 = 1 */
    ASSERT_EQ((written >> 9) & 7u, (unsigned)ADS1015_PGA_4096);
    /* bit 8: MODE = 1 (single-shot) */
    ASSERT_EQ((written >> 8) & 1u, 1u);
    /* bits 7:5: DR = RATE_920 = 3 */
    ASSERT_EQ((written >> 5) & 7u, (unsigned)ADS1015_RATE_920);
}

TEST(test_read_all_four_channels)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    cfg.pga = ADS1015_PGA_2048;  /* 1.0 mV/LSB */
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);

    ads1015_mux_t channels[] = {
        ADS1015_MUX_AIN0_GND, ADS1015_MUX_AIN1_GND,
        ADS1015_MUX_AIN2_GND, ADS1015_MUX_AIN3_GND
    };
    int16_t values[] = {100, 200, -100, 0};
    for (int i = 0; i < 4; i++) {
        mock_conv_12bit = values[i];
        float mv;
        ASSERT_EQ(ads1015_read_single(&dev, channels[i], &mv), ADS1015_OK);
        ASSERT_NEAR(mv, (float)values[i] * 1.0f, 1.0f);
    }
}

TEST(test_read_raw_bad_channel)
{
    mock_reset();
    ads1015_t dev;
    ads1015_cfg_t cfg = default_cfg();
    ASSERT_EQ(ads1015_init(&dev, &mock_i2c, &cfg), ADS1015_OK);
    int16_t raw;
    ASSERT_EQ(ads1015_read_raw(&dev, (ads1015_mux_t)8, &raw), ADS1015_ERR_PARAM);
}

/* ---- a device init refused ---- */

static void assert_device_refused(ads1015_t *d)
{
    int16_t raw;
    float mv;
    ASSERT_EQ(ads1015_read_raw(d, ADS1015_MUX_AIN0_GND, &raw), ADS1015_ERR_PARAM);
    ASSERT_EQ(ads1015_read_single(d, ADS1015_MUX_AIN0_GND, &mv), ADS1015_ERR_PARAM);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const ads1015_i2c_t no_callbacks;
    ads1015_t d;
    ads1015_cfg_t cfg = default_cfg();
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(ads1015_init(&d, NULL, &cfg), ADS1015_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(ads1015_init(&d, &no_callbacks, &cfg), ADS1015_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("ads1015")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_bad_pga);
    RUN(test_init_bad_rate);
    RUN(test_read_raw_positive);
    RUN(test_read_raw_negative);
    RUN(test_read_raw_full_scale);
    RUN(test_read_single_voltage);
    RUN(test_read_single_full_scale);
    RUN(test_i2c_write_failure);
    RUN(test_busy_timeout);
    RUN(test_set_pga);
    RUN(test_set_rate);
    RUN(test_config_bits);
    RUN(test_read_all_four_channels);
    RUN(test_read_raw_bad_channel);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
