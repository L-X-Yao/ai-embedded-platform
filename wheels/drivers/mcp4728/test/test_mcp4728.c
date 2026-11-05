// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "mcp4728.h"

/* ---------- MCP4728-aware mock I2C ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static uint16_t mock_ch_value[4];
static uint8_t  mock_ch_config[4];  /* VREF PD1 PD0 Gx of each channel */
static uint8_t  mock_last_cmd;
static uint8_t  mock_last_buf[16];
static int      mock_last_buf_len;
static int      mock_write_count;

static void mock_reset(void)
{
    mock_fail_next    = 0;
    mock_op_count     = 0;
    mock_fail_at      = -1;
    memset(mock_ch_value, 0, sizeof(mock_ch_value));
    memset(mock_ch_config, 0x08, sizeof(mock_ch_config));  /* factory VREF = 1 */
    mock_last_cmd     = 0;
    memset(mock_last_buf, 0, sizeof(mock_last_buf));
    mock_last_buf_len = 0;
    mock_write_count  = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_regs(void *ctx, uint8_t dev_addr,
                            uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    mock_write_count++;

    mock_last_cmd = reg;
    if (len <= (uint16_t)sizeof(mock_last_buf)) {
        memcpy(mock_last_buf, buf, len);
        mock_last_buf_len = (int)len;
    }

    /* DS22187E Figure 5-8: Multi-Write is 0 1 0 0 0 DAC1 DAC0 UDAC, then
     * VREF PD1 PD0 Gx D11-D8, then D7-D0. Bytes 2-4 repeat per channel. */
    if ((reg & 0xF8u) == 0x40u && len >= 2 && (len - 2u) % 3u == 0) {
        uint8_t cmd = reg;
        for (uint16_t i = 0; ; i = (uint16_t)(i + 3u)) {
            uint8_t ch = (uint8_t)((cmd >> 1) & 0x03u);
            mock_ch_config[ch] = (uint8_t)(buf[i] >> 4);
            mock_ch_value[ch]  = (uint16_t)(((uint16_t)(buf[i] & 0x0Fu) << 8) |
                                            (uint16_t)buf[i + 1u]);
            if ((uint16_t)(i + 2u) >= len) break;
            cmd = buf[i + 2u];
        }
    }
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len; i++) buf[i] = 0;
    return true;
}

static const mcp4728_i2c_t mock_i2c = {
    .write_regs = mock_write_regs,
    .read_regs  = mock_read_regs,
    .ctx        = NULL,
};

static mcp4728_t dev;

static const mcp4728_cfg_t default_cfg = {
    .addr = MCP4728_ADDR_DEFAULT,
};

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(mcp4728_init(&dev, &mock_i2c, &default_cfg), MCP4728_OK);
}

TEST(test_null_safety)
{
    mock_reset();
    ASSERT_EQ(mcp4728_init(NULL, &mock_i2c, &default_cfg), MCP4728_ERR_PARAM);
    ASSERT_EQ(mcp4728_init(&dev, NULL, &default_cfg), MCP4728_ERR_PARAM);
    ASSERT_EQ(mcp4728_init(&dev, &mock_i2c, NULL), MCP4728_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    mcp4728_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    mcp4728_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(mcp4728_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_null_write_channel)
{
    ASSERT_EQ(mcp4728_write_channel(NULL, 0, 0), MCP4728_ERR_PARAM);
}

TEST(test_null_write_all)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(mcp4728_write_all(NULL, (const uint16_t[]){0, 0, 0, 0}),
              MCP4728_ERR_PARAM);
    ASSERT_EQ(mcp4728_write_all(&dev, NULL), MCP4728_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0, (int)MCP4728_OK);
    ASSERT_EQ(-1, (int)MCP4728_ERR_I2C);
    ASSERT_EQ(-2, (int)MCP4728_ERR_PARAM);
}

/* ---------- write_channel tests ---------- */

TEST(test_write_channel_a_zero)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 0), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[0], 0);
    ASSERT_EQ(mock_last_cmd, 0x40);
    ASSERT_EQ(mock_last_buf[0], 0x00);
    ASSERT_EQ(mock_last_buf[1], 0x00);
}

TEST(test_write_channel_a_midscale)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[0], 2048);
    /* 2048 = 0x800: buf[0] = 0x08, buf[1] = 0x00 */
    ASSERT_EQ(mock_last_buf[0], 0x08);
    ASSERT_EQ(mock_last_buf[1], 0x00);
}

TEST(test_write_channel_a_max)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 4095), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[0], 4095);
    ASSERT_EQ(mock_last_buf[0], 0x0F);
    ASSERT_EQ(mock_last_buf[1], 0xFF);
}

TEST(test_write_channel_b)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 1, 1000), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[1], 1000);
    /* Channel B command: 0x40 | (1 << 1) = 0x42 */
    ASSERT_EQ(mock_last_cmd, 0x42);
}

TEST(test_write_channel_c)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 2, 2000), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[2], 2000);
    /* Channel C command: 0x40 | (2 << 1) = 0x44 */
    ASSERT_EQ(mock_last_cmd, 0x44);
}

TEST(test_write_channel_d)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 3, 3000), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[3], 3000);
    /* Channel D command: 0x40 | (3 << 1) = 0x46 */
    ASSERT_EQ(mock_last_cmd, 0x46);
}

TEST(test_write_channel_over_max)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 4096), MCP4728_ERR_PARAM);
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 0xFFFF), MCP4728_ERR_PARAM);
}

TEST(test_write_channel_invalid)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 4, 0), MCP4728_ERR_PARAM);
    ASSERT_EQ(mcp4728_write_channel(&dev, 255, 0), MCP4728_ERR_PARAM);
}

TEST(test_write_channel_i2c_fail)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);
    mock_fail_next = 1;
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 1000), MCP4728_ERR_I2C);
}

TEST(test_write_channel_uninit_dev)
{
    mcp4728_t uninit;
    memset(&uninit, 0, sizeof(uninit));
    ASSERT_EQ(mcp4728_write_channel(&uninit, 0, 0), MCP4728_ERR_PARAM);
}

/* ---------- write_all tests ---------- */

TEST(test_write_all_zeros)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    const uint16_t vals[4] = {0, 0, 0, 0};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[0], 0);
    ASSERT_EQ(mock_ch_value[1], 0);
    ASSERT_EQ(mock_ch_value[2], 0);
    ASSERT_EQ(mock_ch_value[3], 0);
}

TEST(test_write_all_different)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    const uint16_t vals[4] = {100, 2000, 3500, 4095};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(mock_ch_value[0], 100);
    ASSERT_EQ(mock_ch_value[1], 2000);
    ASSERT_EQ(mock_ch_value[2], 3500);
    ASSERT_EQ(mock_ch_value[3], 4095);
    ASSERT_EQ(mock_write_count, 1);
}

/* DS22187E Figure 5-8: bytes 2-4 repeat per channel inside one transaction.
 * reg carries channel A's 2nd byte, so the buffer starts at its 3rd byte. */
TEST(test_write_all_single_multi_write_frame)
{
    static const uint8_t expected[11] = {
        0x00u, 0x64u,         /* A: 100 = 0x064 */
        0x42u, 0x07u, 0xD0u,  /* B: DAC1 DAC0 = 01, 2000 = 0x7D0 */
        0x44u, 0x0Du, 0xACu,  /* C: DAC1 DAC0 = 10, 3500 = 0xDAC */
        0x46u, 0x0Fu, 0xFFu,  /* D: DAC1 DAC0 = 11, 4095 = 0xFFF */
    };

    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    const uint16_t vals[4] = {100, 2000, 3500, 4095};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(mock_write_count, 1);
    ASSERT_EQ_HEX(mock_last_cmd, 0x40u);
    ASSERT_EQ(mock_last_buf_len, 11);
    ASSERT_TRUE(memcmp(mock_last_buf, expected, sizeof(expected)) == 0);
}

TEST(test_write_all_max)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    const uint16_t vals[4] = {4095, 4095, 4095, 4095};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(mock_ch_value[i], 4095);
    }
}

TEST(test_write_all_over_max)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    const uint16_t vals[4] = {100, 4096, 200, 300};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_ERR_PARAM);
    /* No I2C writes should have occurred */
    ASSERT_EQ(mock_write_count, 0);
}

TEST(test_write_all_i2c_fail)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    /* The only I2C op fails, and the driver does not try again */
    mock_fail_at = 0;
    const uint16_t vals[4] = {100, 200, 300, 400};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_ERR_I2C);
    ASSERT_EQ(mock_op_count, 1);
    ASSERT_EQ(mock_write_count, 0);
}

TEST(test_write_all_uninit_dev)
{
    mcp4728_t uninit;
    memset(&uninit, 0, sizeof(uninit));
    const uint16_t vals[4] = {0, 0, 0, 0};
    ASSERT_EQ(mcp4728_write_all(&uninit, vals), MCP4728_ERR_PARAM);
}

/* ---------- data encoding verification ---------- */

TEST(test_value_encoding_1234)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    /* 1234 = 0x4D2: buf[0] = 0x04, buf[1] = 0xD2 */
    mcp4728_write_channel(&dev, 0, 1234);
    ASSERT_EQ(mock_last_buf[0], 0x04);
    ASSERT_EQ(mock_last_buf[1], 0xD2);
}

TEST(test_value_encoding_1)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    /* 1 = 0x001: buf[0] = 0x00, buf[1] = 0x01 */
    mcp4728_write_channel(&dev, 0, 1);
    ASSERT_EQ(mock_last_buf[0], 0x00);
    ASSERT_EQ(mock_last_buf[1], 0x01);
}

/* ---------- configuration bits ---------- */

/* DS22187E Table 4-2 Notes 2 and 3 and Figure 5-8: the upper nibble of the
 * 3rd byte is VREF PD1 PD0 Gx, and 0000 means VDD, normal mode, gain x1. */
TEST(test_write_channel_selects_vdd_gain1_normal)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    ASSERT_EQ(mcp4728_write_channel(&dev, 2, 4095), MCP4728_OK);
    ASSERT_EQ_HEX(mock_ch_config[2], 0x0u);
    /* Only the selected channel is affected (section 5.6.2) */
    ASSERT_EQ_HEX(mock_ch_config[0], 0x8u);
    ASSERT_EQ_HEX(mock_ch_config[1], 0x8u);
    ASSERT_EQ_HEX(mock_ch_config[3], 0x8u);
}

/* ---------- channel independence ---------- */

TEST(test_channels_independent)
{
    mock_reset();
    mcp4728_init(&dev, &mock_i2c, &default_cfg);

    mcp4728_write_channel(&dev, 0, 100);
    mcp4728_write_channel(&dev, 1, 200);
    mcp4728_write_channel(&dev, 2, 300);
    mcp4728_write_channel(&dev, 3, 400);

    ASSERT_EQ(mock_ch_value[0], 100);
    ASSERT_EQ(mock_ch_value[1], 200);
    ASSERT_EQ(mock_ch_value[2], 300);
    ASSERT_EQ(mock_ch_value[3], 400);

    /* Overwrite channel B only */
    mcp4728_write_channel(&dev, 1, 999);
    ASSERT_EQ(mock_ch_value[0], 100);
    ASSERT_EQ(mock_ch_value[1], 999);
    ASSERT_EQ(mock_ch_value[2], 300);
    ASSERT_EQ(mock_ch_value[3], 400);
}

TEST_MAIN("mcp4728")
    RUN(test_init_ok);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_null_write_channel);
    RUN(test_null_write_all);
    RUN(test_error_codes);
    RUN(test_write_channel_a_zero);
    RUN(test_write_channel_a_midscale);
    RUN(test_write_channel_a_max);
    RUN(test_write_channel_b);
    RUN(test_write_channel_c);
    RUN(test_write_channel_d);
    RUN(test_write_channel_over_max);
    RUN(test_write_channel_invalid);
    RUN(test_write_channel_i2c_fail);
    RUN(test_write_channel_uninit_dev);
    RUN(test_write_all_zeros);
    RUN(test_write_all_different);
    RUN(test_write_all_single_multi_write_frame);
    RUN(test_write_all_max);
    RUN(test_write_all_over_max);
    RUN(test_write_all_i2c_fail);
    RUN(test_write_all_uninit_dev);
    RUN(test_value_encoding_1234);
    RUN(test_value_encoding_1);
    RUN(test_write_channel_selects_vdd_gain1_normal);
    RUN(test_channels_independent);
TEST_MAIN_END()
