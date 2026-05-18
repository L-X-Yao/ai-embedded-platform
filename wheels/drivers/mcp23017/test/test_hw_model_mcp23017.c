// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mcp23017.c — tests using MCP23017 hardware register model
 *
 * Validates MCP23017 driver against a datasheet-accurate register model.
 * Tests direction register protection, pin read/write with direction
 * enforcement, OLAT vs GPIO behavior, and interrupt flag reads.
 */

#include "test_harness.h"
#include "hw_model_mcp23017.h"
#include "../mcp23017.h"

static mcp23017_model_t model;

static const mcp23017_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = mcp23017_model_delay,
    .ctx       = &model.hw,
};

/* --- Fail-injecting I2C stubs for error path coverage ------------------- */

static bool fail_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)val;
    return false;
}

static bool fail_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)buf; (void)len;
    return false;
}

/* I2C that always fails writes */
static const mcp23017_i2c_t fail_write_i2c = {
    .write_reg = fail_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

/* I2C that always fails reads */
static const mcp23017_i2c_t fail_read_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = fail_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    mcp23017_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_resets_iodir_to_input)
{
    setup();
    mcp23017_t dev;
    mcp23017_err_t err = mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(err, MCP23017_OK);
    /* After init, IODIR should be 0xFF (all input) per POR reset */
    ASSERT_EQ(model.hw.regs[MCP23017_REG_IODIRA], 0xFF);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_IODIRB], 0xFF);
}

TEST(test_init_clears_other_registers)
{
    setup();
    /* Pre-set some registers to non-zero */
    model.hw.regs[MCP23017_REG_GPPUA] = 0xAA;
    model.hw.regs[MCP23017_REG_OLATA] = 0x55;

    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    /* Init should write 0x00 to all non-IODIR registers */
    ASSERT_EQ(model.hw.regs[MCP23017_REG_GPPUA], 0x00);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_OLATA], 0x00);
}

/* --- direction --------------------------------------------------------- */

TEST(test_set_direction_output)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);

    /* Set port A pins 0-3 as output (IODIR bits = 0 for output) */
    mcp23017_err_t err = mcp23017_set_direction(&dev, MCP23017_PORT_A, 0xF0);
    ASSERT_EQ(err, MCP23017_OK);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_IODIRA], 0xF0);
}

TEST(test_set_direction_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);

    mcp23017_set_direction(&dev, MCP23017_PORT_B, 0x0F);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_IODIRB], 0x0F);
}

/* --- output tests ------------------------------------------------------ */

TEST(test_write_port_a)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00); /* all output */

    mcp23017_err_t err = mcp23017_write_port(&dev, MCP23017_PORT_A, 0xA5);
    ASSERT_EQ(err, MCP23017_OK);
    /* Pin state should reflect written value on output pins */
    ASSERT_EQ(model.pin_state_a, 0xA5);
}

TEST(test_write_pin_rmw)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00); /* all output */

    mcp23017_write_pin(&dev, MCP23017_PORT_A, 0, true);
    mcp23017_write_pin(&dev, MCP23017_PORT_A, 3, true);
    /* Pins 0 and 3 should be set, via read-modify-write on OLAT */
    ASSERT_EQ(model.hw.regs[MCP23017_REG_OLATA] & 0x09, 0x09);
}

/* --- input tests ------------------------------------------------------- */

TEST(test_read_port_returns_pin_state)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    /* Port A default: all input */

    /* Simulate external input */
    mcp23017_model_set_pin_input_a(&model, 0xC3);

    uint8_t value = 0;
    mcp23017_err_t err = mcp23017_read_port(&dev, MCP23017_PORT_A, &value);
    ASSERT_EQ(err, MCP23017_OK);
    ASSERT_EQ(value, 0xC3);
}

TEST(test_read_pin_individual)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);

    mcp23017_model_set_pin_input_a(&model, 0x04); /* only pin 2 high */

    bool level = false;
    mcp23017_read_pin(&dev, MCP23017_PORT_A, 2, &level);
    ASSERT_TRUE(level);

    mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, &level);
    ASSERT_FALSE(level);
}

/* --- pullup ------------------------------------------------------------ */

TEST(test_pullup_sets_register)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);

    mcp23017_err_t err = mcp23017_set_pullup(&dev, MCP23017_PORT_A, 0xFF);
    ASSERT_EQ(err, MCP23017_OK);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_GPPUA], 0xFF);
}

/* --- interrupt --------------------------------------------------------- */

TEST(test_interrupt_enable_mask)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);

    mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0x0F);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_GPINTENA], 0x0F);
}

TEST(test_interrupt_flag_readonly)
{
    setup();
    /* INTF registers are read-only: writes via I2C should be ignored */
    hw_model__write_reg(&model.hw, MCP23017_REG_INTFA, 0xFF);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_INTFA], 0x00);

    /* But model can set it for simulation */
    mcp23017_model_set_intf(&model, true, 0x42);

    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    /* Note: init resets registers, so re-set the flag */
    mcp23017_model_set_intf(&model, true, 0x42);

    uint8_t flags = 0;
    mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, &flags);
    ASSERT_EQ(flags, 0x42);
}

/* --- register protection ----------------------------------------------- */

TEST(test_intcap_readonly)
{
    setup();
    model.hw.regs[MCP23017_REG_INTCAPA] = 0xAB;
    hw_model__write_reg(&model.hw, MCP23017_REG_INTCAPA, 0x00);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_INTCAPA], 0xAB);
}

/* --- init param guard tests -------------------------------------------- */

TEST(test_init_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_init(NULL, &model_i2c, 0x20), MCP23017_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    setup();
    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, NULL, 0x20), MCP23017_ERR_PARAM);
}

TEST(test_init_addr_too_low)
{
    setup();
    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, &model_i2c, 0x1F), MCP23017_ERR_PARAM);
}

TEST(test_init_addr_too_high)
{
    setup();
    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, &model_i2c, 0x28), MCP23017_ERR_PARAM);
}

TEST(test_init_i2c_write_fail)
{
    setup();
    mcp23017_t dev;
    /* fail_write_i2c always returns false from write_reg */
    ASSERT_EQ(mcp23017_init(&dev, &fail_write_i2c, 0x20), MCP23017_ERR_I2C);
}

/* --- set_direction param/i2c failure ------------------------------------ */

TEST(test_set_direction_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_set_direction(NULL, MCP23017_PORT_A, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_direction_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_set_direction(&dev, (mcp23017_port_t)9, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_direction_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00), MCP23017_ERR_I2C);
}

/* --- set_pullup param/i2c failure --------------------------------------- */

TEST(test_set_pullup_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_set_pullup(NULL, MCP23017_PORT_A, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_pullup_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_set_pullup(&dev, (mcp23017_port_t)5, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_pullup_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_A, 0xFF), MCP23017_ERR_I2C);
    dev.i2c = &model_i2c;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_B, 0xFF), MCP23017_ERR_I2C);
}

/* --- write_port param/i2c failure --------------------------------------- */

TEST(test_write_port_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_write_port(NULL, MCP23017_PORT_A, 0x00), MCP23017_ERR_PARAM);
}

TEST(test_write_port_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_write_port(&dev, (mcp23017_port_t)3, 0x00), MCP23017_ERR_PARAM);
}

TEST(test_write_port_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_write_port(&dev, MCP23017_PORT_A, 0xAA), MCP23017_ERR_I2C);
}

/* --- read_port param/i2c failure ---------------------------------------- */

TEST(test_read_port_null_dev)
{
    setup();
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(NULL, MCP23017_PORT_A, &val), MCP23017_ERR_PARAM);
}

TEST(test_read_port_null_value)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAM);
}

TEST(test_read_port_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, (mcp23017_port_t)7, &val), MCP23017_ERR_PARAM);
}

TEST(test_read_port_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_read_i2c;
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, &val), MCP23017_ERR_I2C);
}

/* --- write_pin param/i2c failure ---------------------------------------- */

TEST(test_write_pin_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_write_pin(NULL, MCP23017_PORT_A, 0, true), MCP23017_ERR_PARAM);
}

TEST(test_write_pin_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_write_pin(&dev, (mcp23017_port_t)2, 0, true), MCP23017_ERR_PARAM);
}

TEST(test_write_pin_out_of_range)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 8, true), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 255, true), MCP23017_ERR_PARAM);
}

TEST(test_write_pin_read_step_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_read_i2c;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 0, true), MCP23017_ERR_I2C);
}

TEST(test_write_pin_write_step_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    /* read_regs uses hw_model (succeeds), write_reg fails */
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_B, 1, true), MCP23017_ERR_I2C);
}

TEST(test_write_pin_clear_bit)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00); /* all output */
    /* Set all bits, then clear bit 4 */
    mcp23017_write_port(&dev, MCP23017_PORT_A, 0xFF);
    /* Write via OLAT: need to clear OLAT first */
    model.hw.regs[MCP23017_REG_OLATA] = 0xFF;
    mcp23017_err_t err = mcp23017_write_pin(&dev, MCP23017_PORT_A, 4, false);
    ASSERT_EQ(err, MCP23017_OK);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_OLATA] & (1u << 4), 0x00);
}

/* --- read_pin param/i2c failure ----------------------------------------- */

TEST(test_read_pin_null_dev)
{
    setup();
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(NULL, MCP23017_PORT_A, 0, &level), MCP23017_ERR_PARAM);
}

TEST(test_read_pin_null_level)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, NULL), MCP23017_ERR_PARAM);
}

TEST(test_read_pin_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(&dev, (mcp23017_port_t)4, 0, &level), MCP23017_ERR_PARAM);
}

TEST(test_read_pin_out_of_range)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 8, &level), MCP23017_ERR_PARAM);
}

TEST(test_read_pin_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_read_i2c;
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, &level), MCP23017_ERR_I2C);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_B, 0, &level), MCP23017_ERR_I2C);
}

/* --- set_interrupt param/i2c failure ------------------------------------ */

TEST(test_set_interrupt_null_dev)
{
    setup();
    ASSERT_EQ(mcp23017_set_interrupt(NULL, MCP23017_PORT_A, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_interrupt_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, (mcp23017_port_t)8, 0xFF), MCP23017_ERR_PARAM);
}

TEST(test_set_interrupt_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0xFF), MCP23017_ERR_I2C);
    dev.i2c = &model_i2c;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_write_i2c;
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_B, 0xFF), MCP23017_ERR_I2C);
}

/* --- read_interrupt_flag param/i2c failure ------------------------------ */

TEST(test_read_interrupt_flag_null_dev)
{
    setup();
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(NULL, MCP23017_PORT_A, &flags), MCP23017_ERR_PARAM);
}

TEST(test_read_interrupt_flag_null_flags)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAM);
}

TEST(test_read_interrupt_flag_invalid_port)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, (mcp23017_port_t)6, &flags), MCP23017_ERR_PARAM);
}

TEST(test_read_interrupt_flag_i2c_fail)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    dev.i2c = &fail_read_i2c;
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, &flags), MCP23017_ERR_I2C);
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_B, &flags), MCP23017_ERR_I2C);
}

/* --- port B variants for additional branch coverage -------------------- */

TEST(test_read_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_model_set_pin_input_b(&model, 0x7E);
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_B, &val), MCP23017_OK);
    ASSERT_EQ(val, 0x7E);
}

TEST(test_read_pin_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_model_set_pin_input_b(&model, 0x08); /* pin 3 high */
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_B, 3, &level), MCP23017_OK);
    ASSERT_TRUE(level);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_B, 0, &level), MCP23017_OK);
    ASSERT_FALSE(level);
}

TEST(test_set_pullup_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_B, 0xCC), MCP23017_OK);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_GPPUB], 0xCC);
}

TEST(test_set_interrupt_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_B, 0x33), MCP23017_OK);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_GPINTENB], 0x33);
}

TEST(test_read_interrupt_flag_port_b)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_model_set_intf(&model, false, 0x5A);
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_B, &flags), MCP23017_OK);
    ASSERT_EQ(flags, 0x5A);
}

TEST_MAIN("mcp23017 hardware model")
    RUN(test_init_resets_iodir_to_input);
    RUN(test_init_clears_other_registers);
    RUN(test_set_direction_output);
    RUN(test_set_direction_port_b);
    RUN(test_write_port_a);
    RUN(test_write_pin_rmw);
    RUN(test_read_port_returns_pin_state);
    RUN(test_read_pin_individual);
    RUN(test_pullup_sets_register);
    RUN(test_interrupt_enable_mask);
    RUN(test_interrupt_flag_readonly);
    RUN(test_intcap_readonly);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_addr_too_low);
    RUN(test_init_addr_too_high);
    RUN(test_init_i2c_write_fail);
    RUN(test_set_direction_null_dev);
    RUN(test_set_direction_invalid_port);
    RUN(test_set_direction_i2c_fail);
    RUN(test_set_pullup_null_dev);
    RUN(test_set_pullup_invalid_port);
    RUN(test_set_pullup_i2c_fail);
    RUN(test_write_port_null_dev);
    RUN(test_write_port_invalid_port);
    RUN(test_write_port_i2c_fail);
    RUN(test_read_port_null_dev);
    RUN(test_read_port_null_value);
    RUN(test_read_port_invalid_port);
    RUN(test_read_port_i2c_fail);
    RUN(test_write_pin_null_dev);
    RUN(test_write_pin_invalid_port);
    RUN(test_write_pin_out_of_range);
    RUN(test_write_pin_read_step_fail);
    RUN(test_write_pin_write_step_fail);
    RUN(test_write_pin_clear_bit);
    RUN(test_read_pin_null_dev);
    RUN(test_read_pin_null_level);
    RUN(test_read_pin_invalid_port);
    RUN(test_read_pin_out_of_range);
    RUN(test_read_pin_i2c_fail);
    RUN(test_set_interrupt_null_dev);
    RUN(test_set_interrupt_invalid_port);
    RUN(test_set_interrupt_i2c_fail);
    RUN(test_read_interrupt_flag_null_dev);
    RUN(test_read_interrupt_flag_null_flags);
    RUN(test_read_interrupt_flag_invalid_port);
    RUN(test_read_interrupt_flag_i2c_fail);
    RUN(test_read_port_b);
    RUN(test_read_pin_port_b);
    RUN(test_set_pullup_port_b);
    RUN(test_set_interrupt_port_b);
    RUN(test_read_interrupt_flag_port_b);
TEST_MAIN_END()
