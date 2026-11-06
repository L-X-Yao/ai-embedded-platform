// SPDX-License-Identifier: MIT
/*
 * prop_tsl2591.c -- property-based tests for tsl2591
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_tsl2591.c ../tsl2591.c -o prop_tsl2591 -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "../tsl2591.h"

#define CMD(reg) ((uint8_t)(TSL2591_CMD_BIT | (reg)))

static const tsl2591_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    mock_regs[CMD(TSL2591_REG_ID)] = TSL2591_DEVICE_ID;
    mock_regs[CMD(TSL2591_REG_STATUS)] = 0x01;  /* AVALID: a result is there */
}

static void mock_set_channels(uint16_t ch0, uint16_t ch1)
{
    mock_regs[CMD(TSL2591_REG_C0DATAL)] = (uint8_t)(ch0 & 0xFF);
    mock_regs[CMD(TSL2591_REG_C0DATAH)] = (uint8_t)(ch0 >> 8);
    mock_regs[CMD(TSL2591_REG_C1DATAL)] = (uint8_t)(ch1 & 0xFF);
    mock_regs[CMD(TSL2591_REG_C1DATAH)] = (uint8_t)(ch1 >> 8);
}

static const tsl2591_gain_t valid_gains[] = {
    TSL2591_GAIN_LOW, TSL2591_GAIN_MEDIUM, TSL2591_GAIN_HIGH, TSL2591_GAIN_MAX,
};

static const tsl2591_itime_t valid_itimes[] = {
    TSL2591_ITIME_100MS, TSL2591_ITIME_200MS, TSL2591_ITIME_300MS,
    TSL2591_ITIME_400MS, TSL2591_ITIME_500MS, TSL2591_ITIME_600MS,
};

/* --- init_no_crash: random register values never crash init --- */
PROPERTY(init_no_crash, 10000) {
    mock_i2c_reset();
    ARBI_FILL(seed, mock_regs, 256);
    tsl2591_t dev;
    (void)tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS);
    PROP_ASSERT(1);
}

/* --- lux_non_negative: lux is always >= 0 --- */
PROPERTY(lux_non_negative, 10000) {
    mock_reset_chip();
    tsl2591_t dev;
    /* Pick random valid gain and itime */
    tsl2591_gain_t gain = valid_gains[ARBI_RANGE(seed, 0, 3)];
    tsl2591_itime_t itime = valid_itimes[ARBI_RANGE(seed, 0, 5)];
    tsl2591_init(&dev, &mock_i2c, gain, itime);

    /* Random channel values */
    uint16_t ch0 = ARBI_UINT16(seed);
    uint16_t ch1 = ARBI_UINT16(seed);
    mock_set_channels(ch0, ch1);

    tsl2591_data_t data;
    tsl2591_err_t err = tsl2591_read(&dev, &data);
    PROP_ASSERT(err == TSL2591_OK);
    PROP_ASSERT(data.lux >= 0.0f);
}

/* --- double_init_safe: repeated init is safe --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset_chip();
    tsl2591_t dev;
    tsl2591_gain_t gain = valid_gains[ARBI_RANGE(seed, 0, 3)];
    tsl2591_itime_t itime = valid_itimes[ARBI_RANGE(seed, 0, 5)];
    tsl2591_err_t e1 = tsl2591_init(&dev, &mock_i2c, gain, itime);
    tsl2591_err_t e2 = tsl2591_init(&dev, &mock_i2c, gain, itime);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(e1, TSL2591_OK);
}

/* --- config_variants: all gain/itime combos work --- */
PROPERTY(config_variants, 10000) {
    mock_reset_chip();
    tsl2591_t dev;
    tsl2591_gain_t gain = valid_gains[ARBI_RANGE(seed, 0, 3)];
    tsl2591_itime_t itime = valid_itimes[ARBI_RANGE(seed, 0, 5)];

    tsl2591_err_t err = tsl2591_init(&dev, &mock_i2c, gain, itime);
    PROP_ASSERT(err == TSL2591_OK);

    /* Verify stored gain and itime */
    PROP_ASSERT_EQ(dev.gain, gain);
    PROP_ASSERT_EQ(dev.itime, itime);
}

/* --- visible_clamped: visible = max(ch0 - ch1, 0) --- */
PROPERTY(visible_clamped, 10000) {
    mock_reset_chip();
    tsl2591_t dev;
    tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS);

    uint16_t ch0 = ARBI_UINT16(seed);
    uint16_t ch1 = ARBI_UINT16(seed);
    mock_set_channels(ch0, ch1);

    tsl2591_data_t data;
    tsl2591_read(&dev, &data);

    if (ch0 >= ch1) {
        PROP_ASSERT_EQ(data.visible, (uint32_t)(ch0 - ch1));
    } else {
        PROP_ASSERT_EQ(data.visible, 0);
    }
}

TEST_MAIN("tsl2591 properties") {
    RUN(init_no_crash);
    RUN(lux_non_negative);
    RUN(double_init_safe);
    RUN(config_variants);
    RUN(visible_clamped);
}
TEST_MAIN_END()
