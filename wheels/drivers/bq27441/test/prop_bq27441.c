// SPDX-License-Identifier: MIT
/*
 * prop_bq27441.c — property-based tests for bq27441
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_bq27441.c ../bq27441.c -o prop_bq27441
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "bq27441.h"

/* BQ27441 CONTROL register sub-command behavior */
#define REG_CONTROL 0x00u

static uint8_t mock_control_low;

static bool mock_bq_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    if (!mock_i2c_write_reg(ctx, dev_addr, reg, val))
        return false;

    /* Simulate DEVICE_TYPE sub-command response: low byte, then high byte */
    if (reg == REG_CONTROL) {
        mock_control_low = val;
    } else if (reg == REG_CONTROL + 1) {
        uint16_t subcmd = (uint16_t)((uint16_t)val << 8 | mock_control_low);
        if (subcmd == 0x0001u) {
            mock_regs[REG_CONTROL]     = 0x21;
            mock_regs[REG_CONTROL + 1] = 0x04;
        }
    }
    return true;
}

static const bq27441_i2c_t prop_i2c = {
    .write_reg  = mock_bq_write_reg,
    .read_regs  = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

static void mock_set_le16(uint8_t reg, uint16_t val)
{
    mock_regs[reg]     = (uint8_t)(val & 0xFFu);
    mock_regs[reg + 1] = (uint8_t)(val >> 8);
}

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Plant valid device type response for CONTROL sub-command */
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;

    bq27441_t dev;
    bq27441_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Valid device ID */
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;

    bq27441_t dev;
    bq27441_err_t err = bq27441_init(&dev, &prop_i2c);
    if (err != BQ27441_OK) return;

    bq27441_data_t data;
    mock_regs[0x06] = 0x08; /* BAT_DET */
    bq27441_read(&dev, &data);
    /* Must not crash with arbitrary register values */
}

PROPERTY(voltage_read_consistent, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;

    uint16_t planted_mv = ARBI_UINT16(seed);
    mock_set_le16(0x04, planted_mv);

    bq27441_t dev;
    bq27441_err_t err = bq27441_init(&dev, &prop_i2c);
    if (err != BQ27441_OK) return;

    uint16_t read_mv = 0;
    err = bq27441_read_voltage(&dev, &read_mv);
    if (err != BQ27441_OK) return;

    PROP_ASSERT_EQ(read_mv, planted_mv);
}

PROPERTY(soc_read_consistent, 5000)
{
    mock_i2c_reset();
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;

    uint16_t planted_soc = ARBI_UINT16(seed);
    mock_set_le16(0x1C, planted_soc);

    bq27441_t dev;
    bq27441_err_t err = bq27441_init(&dev, &prop_i2c);
    if (err != BQ27441_OK) return;

    uint16_t read_soc = 0;
    err = bq27441_read_soc(&dev, &read_soc);
    if (err != BQ27441_OK) return;

    PROP_ASSERT_EQ(read_soc, planted_soc);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;

    bq27441_t dev;
    bq27441_init(&dev, &prop_i2c);
    /* Second init */
    mock_i2c_reset();
    mock_regs[REG_CONTROL]     = 0x21;
    mock_regs[REG_CONTROL + 1] = 0x04;
    bq27441_init(&dev, &prop_i2c);
    /* Must not crash */
}

TEST_MAIN("bq27441 properties")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(voltage_read_consistent);
    RUN(soc_read_consistent);
    RUN(double_init_safe);
TEST_MAIN_END()
