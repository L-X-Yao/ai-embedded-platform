// SPDX-License-Identifier: MIT
/*
 * prop_max30102.c -- property-based tests for max30102
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_max30102.c ../max30102.c -o prop_max30102
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "../max30102.h"

#define REG_PART_ID      0xFFu
#define REG_FIFO_WR_PTR  0x04u
#define REG_FIFO_RD_PTR  0x06u
#define REG_FIFO_DATA    0x07u
#define REG_MODE_CONFIG  0x09u
#define REG_TEMP_INT     0x1Fu
#define REG_TEMP_FRAC    0x20u

static const max30102_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    mock_regs[REG_PART_ID] = 0x15;
}

/* --- init_no_crash: random register values never crash init --- */
PROPERTY(init_no_crash, 10000) {
    mock_i2c_reset();
    ARBI_FILL(seed, mock_regs, 256);
    max30102_t dev;
    (void)max30102_init(&dev, &mock_i2c);
    PROP_ASSERT(1);
}

/* --- temp_read_in_range: temperature always in physical range --- */
PROPERTY(temp_read_in_range, 10000) {
    mock_reset_chip();
    max30102_t dev;
    max30102_init(&dev, &mock_i2c);

    /* Random temperature register values */
    mock_regs[REG_TEMP_INT]  = ARBI_UINT8(seed);
    mock_regs[REG_TEMP_FRAC] = ARBI_UINT8(seed) & 0x0F; /* 4-bit fraction */

    float temp;
    max30102_err_t err = max30102_read_temp(&dev, &temp);
    PROP_ASSERT(err == MAX30102_OK);
    /* int8_t range: -128 to 127, plus 0..15*0.0625 fraction */
    PROP_ASSERT(temp >= -128.0f);
    PROP_ASSERT(temp <= 128.0f);
}

/* --- double_init_safe: repeated init is safe --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset_chip();
    max30102_t dev;
    max30102_err_t e1 = max30102_init(&dev, &mock_i2c);
    max30102_err_t e2 = max30102_init(&dev, &mock_i2c);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(e1, MAX30102_OK);
}

/* --- fifo_empty_returns_zero: empty FIFO always reports 0 samples --- */
PROPERTY(fifo_empty_returns_zero, 10000) {
    mock_reset_chip();
    max30102_t dev;
    max30102_init(&dev, &mock_i2c);

    /* Set wr_ptr == rd_ptr (both random but equal) */
    uint8_t ptr = ARBI_UINT8(seed) & 0x1F;
    mock_regs[REG_FIFO_WR_PTR] = ptr;
    mock_regs[REG_FIFO_RD_PTR] = ptr;

    max30102_sample_t samples[4];
    uint8_t count = 99;
    max30102_err_t err = max30102_read_fifo(&dev, samples, 4, &count);
    PROP_ASSERT(err == MAX30102_OK);
    PROP_ASSERT_EQ(count, 0);
}

/* --- shutdown_preserves_mode_bits: shutdown toggle preserves mode --- */
PROPERTY(shutdown_preserves_mode, 5000) {
    mock_reset_chip();
    max30102_t dev;
    max30102_init(&dev, &mock_i2c);

    /* Pick a random valid mode */
    max30102_mode_t mode = (ARBI_UINT8(seed) & 1) ? MAX30102_MODE_HR : MAX30102_MODE_SPO2;
    max30102_set_mode(&dev, mode);

    uint8_t mode_bits_before = mock_regs[REG_MODE_CONFIG] & 0x07;

    max30102_shutdown(&dev, true);
    PROP_ASSERT((mock_regs[REG_MODE_CONFIG] & 0x07) == mode_bits_before);

    max30102_shutdown(&dev, false);
    PROP_ASSERT((mock_regs[REG_MODE_CONFIG] & 0x07) == mode_bits_before);
}

TEST_MAIN("max30102 properties") {
    RUN(init_no_crash);
    RUN(temp_read_in_range);
    RUN(double_init_safe);
    RUN(fifo_empty_returns_zero);
    RUN(shutdown_preserves_mode);
}
TEST_MAIN_END()
