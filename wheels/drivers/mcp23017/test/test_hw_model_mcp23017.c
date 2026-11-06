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
    .read_regs = mcp23017_hw_model_read_regs,
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
    .read_regs = mcp23017_hw_model_read_regs,
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

/* --- Interrupt hooks ---------------------------------------------------- *
 * These wrappers count writes to the read-only INTF and INTCAP registers,
 * and note whether interrupts were still enabled when a read cleared one.  */

static int     hook_readonly_writes;
static int     hook_fail_read_reg;
static uint8_t hook_gpinten_at_clear;

static bool hook_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    if (reg >= MCP23017_REG_INTFA && reg <= MCP23017_REG_INTCAPB)
        hook_readonly_writes++;
    return hw_model_i2c_write_reg(ctx, dev_addr, reg, val);
}

static bool hook_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    if (reg == hook_fail_read_reg)
        return false;
    if (reg == MCP23017_REG_GPIOA || reg == MCP23017_REG_INTCAPA)
        hook_gpinten_at_clear |= model.hw.regs[MCP23017_REG_GPINTENA];
    if (reg == MCP23017_REG_GPIOB || reg == MCP23017_REG_INTCAPB)
        hook_gpinten_at_clear |= model.hw.regs[MCP23017_REG_GPINTENB];
    return mcp23017_hw_model_read_regs(ctx, dev_addr, reg, buf, len);
}

static const mcp23017_i2c_t hook_i2c = {
    .write_reg = hook_write_reg,
    .read_regs = hook_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    mcp23017_model_init(&model);
    hook_readonly_writes  = 0;
    hook_fail_read_reg    = -1;
    hook_gpinten_at_clear = 0;
}

static void reg_write(uint8_t reg, uint8_t val)
{
    model_i2c.write_reg(&model.hw, 0x20, reg, val);
}

static uint8_t reg_read(uint8_t reg)
{
    uint8_t val = 0;
    model_i2c.read_regs(&model.hw, 0x20, reg, &val, 1);
    return val;
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

TEST(test_init_clears_pending_interrupt)
{
    setup();
    /* An MCU-only reset leaves the part as it was: enabled and pending */
    reg_write(MCP23017_REG_GPINTENA, 0x04);
    reg_write(MCP23017_REG_GPINTENB, 0x80);
    mcp23017_model_set_pin_input_a(&model, 0x04);
    mcp23017_model_set_pin_input_b(&model, 0x80);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    ASSERT_TRUE(mcp23017_model_int_active(&model, false));

    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, &hook_i2c, 0x20), MCP23017_OK);

    ASSERT_EQ(model.hw.regs[MCP23017_REG_INTFA], 0x00);
    ASSERT_EQ(model.hw.regs[MCP23017_REG_INTFB], 0x00);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
    ASSERT_FALSE(mcp23017_model_int_active(&model, false));
    /* Interrupts were off by then, so no new change could re-arm them */
    ASSERT_EQ(hook_gpinten_at_clear, 0x00);
}

TEST(test_init_skips_readonly_registers)
{
    setup();
    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, &hook_i2c, 0x20), MCP23017_OK);
    ASSERT_EQ(hook_readonly_writes, 0);
}

TEST(test_init_interrupt_clear_read_fail)
{
    mcp23017_t dev;

    setup();
    hook_fail_read_reg = MCP23017_REG_GPIOA;
    ASSERT_EQ(mcp23017_init(&dev, &hook_i2c, 0x20), MCP23017_ERR_I2C);

    setup();
    hook_fail_read_reg = MCP23017_REG_GPIOB;
    ASSERT_EQ(mcp23017_init(&dev, &hook_i2c, 0x20), MCP23017_ERR_I2C);
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

    /* Pins that change raise it */
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0x42);
    mcp23017_model_set_pin_input_a(&model, 0x42);

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
    mcp23017_err_t err = mcp23017_write_pin(&dev, MCP23017_PORT_A, 4, false);
    ASSERT_EQ(err, MCP23017_OK);
    ASSERT_EQ_HEX(model.hw.regs[MCP23017_REG_OLATA], 0xEF);
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
    mcp23017_set_interrupt(&dev, MCP23017_PORT_B, 0x5A);
    mcp23017_model_set_pin_input_b(&model, 0x5A);
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_B, &flags), MCP23017_OK);
    ASSERT_EQ(flags, 0x5A);
}

/* --- the model against the manual ------------------------------------- */

/* "Writing to this register modifies the Output Latch (OLAT) register", and
 * "Reading from this register reads the port", p23 */
TEST(test_gpio_write_fills_the_whole_latch)
{
    setup();
    mcp23017_model_set_pin_input_a(&model, 0x0F);
    reg_write(MCP23017_REG_GPIOA, 0xA5);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_OLATA), 0xA5);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0x0F);

    /* The value written before the switch comes out with it */
    reg_write(MCP23017_REG_IODIRA, 0x00);
    ASSERT_EQ_HEX(model.pin_state_a, 0xA5);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0xA5);
}

TEST(test_gpio_read_joins_inputs_and_latch)
{
    setup();
    reg_write(MCP23017_REG_IODIRB, 0xF0);
    reg_write(MCP23017_REG_OLATB, 0x05);
    mcp23017_model_set_pin_input_b(&model, 0xA0);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOB), 0xA5);
}

/* "A write to this register modifies the output latches that modifies the
 * pins configured as outputs", p24, and GPIO "reflects the value on the
 * port", p23 */
TEST(test_olat_write_reaches_gpio)
{
    setup();
    reg_write(MCP23017_REG_IODIRA, 0x00);
    reg_write(MCP23017_REG_OLATA, 0x3C);
    ASSERT_EQ_HEX(model.pin_state_a, 0x3C);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0x3C);
}

/* "A read from this register results in a read of the OLAT and not the port
 * itself", p24 */
TEST(test_olat_read_is_not_the_port)
{
    setup();
    mcp23017_model_set_pin_input_a(&model, 0xFF);
    reg_write(MCP23017_REG_OLATA, 0x00);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_OLATA), 0x00);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0xFF);
}

TEST(test_write_pin_reads_back_on_an_output)
{
    setup();
    mcp23017_t dev;
    mcp23017_init(&dev, &model_i2c, 0x20);
    mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00);

    bool level = false;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 5, true), MCP23017_OK);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 5, &level), MCP23017_OK);
    ASSERT_TRUE(level);
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 5, false), MCP23017_OK);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 5, &level), MCP23017_OK);
    ASSERT_FALSE(level);
}

/* Table 3-1 on p12 gives 0Ah and 0Bh as the one IOCON, and bit 0 is
 * unimplemented, Register 3-6 on p21 */
TEST(test_iocon_is_one_register)
{
    setup();
    reg_write(MCP23017_REG_IOCON_A, 0x40);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_IOCON_B), 0x40);
    reg_write(MCP23017_REG_IOCON_B, 0x20);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_IOCON_A), 0x20);
    reg_write(MCP23017_REG_IOCON_A, 0x01);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_IOCON_B), 0x00);
}

/* "GPIO register bit reflects the opposite logic state of the input pin",
 * Register 3-2 on p18 */
TEST(test_ipol_inverts_inputs_only)
{
    setup();
    reg_write(MCP23017_REG_IODIRA, 0xF0);
    reg_write(MCP23017_REG_OLATA, 0x05);
    mcp23017_model_set_pin_input_a(&model, 0x50);
    reg_write(MCP23017_REG_IPOLA, 0xFF);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0xA5);
}

/* "A set bit indicates that the associated pin caused the interrupt" (p22);
 * INTCAP "captures the GPIO port value at the time the interrupt occurred"
 * (p23) */
TEST(test_ioc_fills_intf_and_intcap)
{
    setup();
    reg_write(MCP23017_REG_GPINTENA, 0x04);
    mcp23017_model_set_pin_input_a(&model, 0x06);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    ASSERT_FALSE(mcp23017_model_int_active(&model, false));
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTFA), 0x04);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTCAPA), 0x06);
}

/* "Only those pins that are configured as an input (IODIR register) with
 * Interrupt-On-Change (IOC) enabled ... can cause an interrupt", p25 */
TEST(test_ioc_needs_an_enabled_input)
{
    setup();
    reg_write(MCP23017_REG_GPINTENA, 0x0F);
    mcp23017_model_set_pin_input_a(&model, 0xF0);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));

    reg_write(MCP23017_REG_IODIRB, 0x00);
    reg_write(MCP23017_REG_GPINTENB, 0xFF);
    reg_write(MCP23017_REG_OLATB, 0xFF);
    ASSERT_FALSE(mcp23017_model_int_active(&model, false));
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTFB), 0x00);
}

/* "Subsequent interrupt conditions on the port will not cause an interrupt
 * to occur as long as the interrupt is not cleared" and "Writing to these
 * registers does not affect the interrupt", p25 */
TEST(test_ioc_holds_until_gpio_or_intcap_is_read)
{
    setup();
    reg_write(MCP23017_REG_GPINTENA, 0xFF);
    mcp23017_model_set_pin_input_a(&model, 0x01);
    mcp23017_model_set_pin_input_a(&model, 0x03);
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTFA), 0x01);
    ASSERT_EQ_HEX(model.hw.regs[MCP23017_REG_INTCAPA], 0x01);

    reg_write(MCP23017_REG_GPIOA, 0x00);
    reg_write(MCP23017_REG_INTCAPA, 0x00);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));

    ASSERT_EQ_HEX(reg_read(MCP23017_REG_GPIOA), 0x03);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTFA), 0x00);
    /* "The register remains unchanged until the interrupt is cleared", and
     * it "is updated only when an interrupt occurs", p23 */
    ASSERT_EQ_HEX(model.hw.regs[MCP23017_REG_INTCAPA], 0x01);

    mcp23017_model_set_pin_input_a(&model, 0x07);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    ASSERT_EQ_HEX(reg_read(MCP23017_REG_INTCAPA), 0x07);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
}

/* "The new initial state for the pin is a logic '0' after the interrupt is
 * cleared", p25 */
TEST(test_ioc_on_pin_change_takes_the_new_level)
{
    setup();
    mcp23017_model_set_pin_input_a(&model, 0x01);
    reg_write(MCP23017_REG_GPINTENA, 0x01);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));

    mcp23017_model_set_pin_input_a(&model, 0x00);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    (void)reg_read(MCP23017_REG_GPIOA);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));

    mcp23017_model_set_pin_input_a(&model, 0x00);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
    mcp23017_model_set_pin_input_a(&model, 0x01);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
}

/* "The interrupt condition will remain as long as the condition exists,
 * regardless if the INTCAP or GPIO is read", p25 */
TEST(test_ioc_from_defval_stays_while_the_pin_differs)
{
    setup();
    reg_write(MCP23017_REG_DEFVALA, 0x01);
    reg_write(MCP23017_REG_INTCONA, 0x01);
    mcp23017_model_set_pin_input_a(&model, 0x01);
    reg_write(MCP23017_REG_GPINTENA, 0x01);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));

    mcp23017_model_set_pin_input_a(&model, 0x00);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    (void)reg_read(MCP23017_REG_GPIOA);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));

    mcp23017_model_set_pin_input_a(&model, 0x01);
    (void)reg_read(MCP23017_REG_GPIOA);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
}

/* "the internal signals are OR'ed together and routed to the INTn pads", p24 */
TEST(test_mirror_joins_the_int_pins)
{
    setup();
    reg_write(MCP23017_REG_GPINTENB, 0x01);
    reg_write(MCP23017_REG_IOCON_A, 0x40);
    mcp23017_model_set_pin_input_b(&model, 0x01);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    ASSERT_TRUE(mcp23017_model_int_active(&model, false));

    /* Table 3-6: GPIOA leaves port B's interrupt, GPIOB clears it */
    (void)reg_read(MCP23017_REG_GPIOA);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));
    (void)reg_read(MCP23017_REG_GPIOB);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
}

/* The whole path through the API */
TEST(test_driver_sees_an_interrupt_and_clears_it)
{
    setup();
    mcp23017_t dev;
    ASSERT_EQ(mcp23017_init(&dev, &model_i2c, 0x20), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0x10), MCP23017_OK);

    mcp23017_model_set_pin_input_a(&model, 0x10);
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, &flags), MCP23017_OK);
    ASSERT_EQ_HEX(flags, 0x10);
    ASSERT_TRUE(mcp23017_model_int_active(&model, true));

    uint8_t value = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, &value), MCP23017_OK);
    ASSERT_EQ_HEX(value, 0x10);
    ASSERT_FALSE(mcp23017_model_int_active(&model, true));
}

TEST_MAIN("mcp23017 hardware model")
    RUN(test_gpio_write_fills_the_whole_latch);
    RUN(test_gpio_read_joins_inputs_and_latch);
    RUN(test_olat_write_reaches_gpio);
    RUN(test_olat_read_is_not_the_port);
    RUN(test_write_pin_reads_back_on_an_output);
    RUN(test_iocon_is_one_register);
    RUN(test_ipol_inverts_inputs_only);
    RUN(test_ioc_fills_intf_and_intcap);
    RUN(test_ioc_needs_an_enabled_input);
    RUN(test_ioc_holds_until_gpio_or_intcap_is_read);
    RUN(test_ioc_on_pin_change_takes_the_new_level);
    RUN(test_ioc_from_defval_stays_while_the_pin_differs);
    RUN(test_mirror_joins_the_int_pins);
    RUN(test_driver_sees_an_interrupt_and_clears_it);
    RUN(test_init_resets_iodir_to_input);
    RUN(test_init_clears_other_registers);
    RUN(test_init_clears_pending_interrupt);
    RUN(test_init_skips_readonly_registers);
    RUN(test_init_interrupt_clear_read_fail);
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
