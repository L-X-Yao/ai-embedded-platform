// SPDX-License-Identifier: MIT
/*
 * prop_pac1952.c -- property-based tests for pac1952
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_pac1952.c ../pac1952.c -o prop_pac1952 -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "../pac1952.h"

#define REG_CTRL       0x01u
#define REG_VBUS1      0x07u
#define REG_VBUS2      0x08u
#define REG_VSENSE1    0x0Bu
#define REG_VSENSE2    0x0Cu
#define REG_PRODUCT_ID 0xFDu

static const pac1952_i2c_t mock_i2c_hal = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    mock_regs[REG_PRODUCT_ID] = PAC1952_PRODUCT_ID_VAL;
}

/* --- init_no_crash: random register values never crash init --- */
PROPERTY(init_no_crash, 10000) {
    mock_i2c_reset();
    ARBI_FILL(seed, mock_regs, 256);
    pac1952_t dev;
    pac1952_cfg_t cfg = { .sense_resistor_mohm = { 10.0f, 10.0f } };
    (void)pac1952_init(&dev, &mock_i2c_hal, &cfg);
    PROP_ASSERT(1);
}

/* --- vbus_in_range: any raw VBUS value yields voltage in [0, 32V] --- */
PROPERTY(vbus_in_range, 10000) {
    mock_reset_chip();
    pac1952_t dev;
    pac1952_cfg_t cfg = { .sense_resistor_mohm = { 10.0f, 10.0f } };
    pac1952_init(&dev, &mock_i2c_hal, &cfg);

    /* Random VBUS register for channel 0 */
    mock_regs[REG_VBUS1]     = ARBI_UINT8(seed);
    mock_regs[REG_VBUS1 + 1] = ARBI_UINT8(seed);
    mock_regs[REG_VSENSE1]     = 0;
    mock_regs[REG_VSENSE1 + 1] = 0;

    pac1952_channel_t ch;
    pac1952_err_t err = pac1952_read_channel(&dev, 0, &ch);
    PROP_ASSERT(err == PAC1952_OK);
    /* VBUS FSR = 32V, unsigned 16-bit */
    PROP_ASSERT(ch.vbus_v >= 0.0f);
    PROP_ASSERT(ch.vbus_v <= 32.1f);
}

/* --- double_init_safe: repeated init does not corrupt state --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset_chip();
    pac1952_t dev;
    pac1952_cfg_t cfg = { .sense_resistor_mohm = { 10.0f, 10.0f } };
    pac1952_err_t e1 = pac1952_init(&dev, &mock_i2c_hal, &cfg);
    pac1952_err_t e2 = pac1952_init(&dev, &mock_i2c_hal, &cfg);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(e1, PAC1952_OK);
}

/* --- config_variants: different resistor values all accepted --- */
PROPERTY(config_variants, 10000) {
    mock_reset_chip();
    pac1952_t dev;

    /* Generate positive resistor values in range [0.1, 1000] mohm */
    float r0 = (float)(ARBI_RANGE(seed, 1, 10000)) / 10.0f;
    float r1 = (float)(ARBI_RANGE(seed, 1, 10000)) / 10.0f;
    pac1952_cfg_t cfg = { .sense_resistor_mohm = { r0, r1 } };

    pac1952_err_t err = pac1952_init(&dev, &mock_i2c_hal, &cfg);
    PROP_ASSERT(err == PAC1952_OK);
}

/* --- current_sign_matches_vsense: current sign matches sense voltage sign --- */
PROPERTY(current_sign_matches_vsense, 10000) {
    mock_reset_chip();
    pac1952_t dev;
    pac1952_cfg_t cfg = { .sense_resistor_mohm = { 10.0f, 10.0f } };
    pac1952_init(&dev, &mock_i2c_hal, &cfg);

    /* Random signed VSENSE for channel 0 */
    int16_t raw_vs = (int16_t)ARBI_UINT16(seed);
    uint16_t u = (uint16_t)raw_vs;
    mock_regs[REG_VSENSE1]     = (uint8_t)(u >> 8);
    mock_regs[REG_VSENSE1 + 1] = (uint8_t)(u & 0xFF);
    mock_regs[REG_VBUS1]       = 0x40; /* some non-zero VBUS */
    mock_regs[REG_VBUS1 + 1]   = 0x00;

    pac1952_channel_t ch;
    pac1952_err_t err = pac1952_read_channel(&dev, 0, &ch);
    PROP_ASSERT(err == PAC1952_OK);

    /* If VSENSE positive, current should be non-negative; if negative, non-positive */
    if (raw_vs > 0) {
        PROP_ASSERT(ch.current_ma >= 0.0f);
    } else if (raw_vs < 0) {
        PROP_ASSERT(ch.current_ma <= 0.0f);
    }
}

TEST_MAIN("pac1952 properties") {
    RUN(init_no_crash);
    RUN(vbus_in_range);
    RUN(double_init_safe);
    RUN(config_variants);
    RUN(current_sign_matches_vsense);
}
TEST_MAIN_END()
