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
TEST_MAIN_END()
