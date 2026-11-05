// SPDX-License-Identifier: MIT
/*
 * prop_mcp4725.c — property-based tests for MCP4725 DAC
 */

#include "test_harness.h"
#include "property_test.h"
#include "mcp4725.h"
#include <string.h>

/* Use ARBI_FILL once to prevent unused-function warning */
__attribute__((unused))
static void prop__touch_fill(void) { uint8_t b; uint32_t s = 1; ARBI_FILL(s, &b, 1); }

/* --- minimal mock for property tests --- */

static uint8_t  prop_last_write[8];
static int      prop_last_write_len;
static uint16_t prop_mock_dac;
static uint8_t  prop_mock_pd;

static bool prop_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (len <= (uint16_t)sizeof(prop_last_write)) {
        memcpy(prop_last_write, data, len);
        prop_last_write_len = (int)len;
    }
    if (len == 2) {
        prop_mock_pd  = (data[0] >> 4) & 0x03u;
        prop_mock_dac = (uint16_t)(((uint16_t)(data[0] & 0x0Fu) << 8) |
                                   (uint16_t)data[1]);
    } else if (len == 3) {
        prop_mock_pd  = (data[0] >> 1) & 0x03u;
        prop_mock_dac = (uint16_t)(((uint16_t)data[1] << 4) |
                                   ((uint16_t)data[2] >> 4));
    }
    return true;
}

static bool prop_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (len >= 5) {
        /* DS22039D Figure 6-3, after the address byte */
        data[0] = (uint8_t)(0xC0u | ((unsigned)prop_mock_pd << 1));
        data[1] = (uint8_t)((prop_mock_dac >> 4) & 0xFFu);
        data[2] = (uint8_t)((prop_mock_dac & 0x0Fu) << 4);
        data[3] = 0;
        data[4] = 0;
    }
    return true;
}

static const mcp4725_i2c_t prop_i2c = {
    .write = prop_write,
    .read  = prop_read,
    .ctx   = NULL,
};

PROPERTY(write_fast_roundtrip, 10000)
{
    prop_mock_dac = 0;
    prop_mock_pd  = 0;

    mcp4725_t dev;
    mcp4725_init(&dev, &prop_i2c, MCP4725_ADDR_A);

    uint16_t val = ARBI_UINT16(seed) & 0x0FFFu;
    mcp4725_err_t err = mcp4725_write_fast(&dev, val);
    PROP_ASSERT_EQ(err, MCP4725_OK);
    PROP_ASSERT_EQ(prop_mock_dac, val);
}

PROPERTY(write_dac_roundtrip, 10000)
{
    prop_mock_dac = 0;
    prop_mock_pd  = 0;

    mcp4725_t dev;
    mcp4725_init(&dev, &prop_i2c, MCP4725_ADDR_A);

    uint16_t val = ARBI_UINT16(seed) & 0x0FFFu;
    uint8_t pd = ARBI_UINT8(seed) & 0x03u;

    mcp4725_err_t err = mcp4725_write_dac(&dev, val, (mcp4725_pd_t)pd);
    PROP_ASSERT_EQ(err, MCP4725_OK);
    PROP_ASSERT_EQ(prop_mock_dac, val);
    PROP_ASSERT_EQ(prop_mock_pd, pd);
}

PROPERTY(write_read_roundtrip, 10000)
{
    prop_mock_dac = 0;
    prop_mock_pd  = 0;

    mcp4725_t dev;
    mcp4725_init(&dev, &prop_i2c, MCP4725_ADDR_A);

    uint16_t val = ARBI_UINT16(seed) & 0x0FFFu;
    uint8_t pd = ARBI_UINT8(seed) & 0x03u;

    mcp4725_write_dac(&dev, val, (mcp4725_pd_t)pd);

    mcp4725_status_t st;
    mcp4725_err_t err = mcp4725_read_status(&dev, &st);
    PROP_ASSERT_EQ(err, MCP4725_OK);
    PROP_ASSERT_EQ(st.dac_value, val);
    PROP_ASSERT_EQ(st.pd_mode, pd);
}

PROPERTY(over_max_rejected, 5000)
{
    mcp4725_t dev;
    prop_mock_dac = 0;
    mcp4725_init(&dev, &prop_i2c, MCP4725_ADDR_A);

    uint16_t val = (uint16_t)(4096u + (ARBI_UINT16(seed) % 60000u));

    PROP_ASSERT_EQ(mcp4725_write_fast(&dev, val), MCP4725_ERR_PARAM);
    PROP_ASSERT_EQ(mcp4725_write_dac(&dev, val, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
    PROP_ASSERT_EQ(mcp4725_write_eeprom(&dev, val, MCP4725_PD_NORMAL), MCP4725_ERR_PARAM);
}

TEST_MAIN("prop_mcp4725")
    RUN(write_fast_roundtrip);
    RUN(write_dac_roundtrip);
    RUN(write_read_roundtrip);
    RUN(over_max_rejected);
TEST_MAIN_END()
