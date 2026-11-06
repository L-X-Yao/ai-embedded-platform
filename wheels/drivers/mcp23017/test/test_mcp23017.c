// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "mcp23017.h"

/* ---- Bank=0 register addresses (mirror driver defines for test) ---- */
#define REG_IODIRA      0x00
#define REG_IODIRB      0x01
#define REG_GPINTENA    0x04
#define REG_GPINTENB    0x05
#define REG_GPPUA       0x0C
#define REG_GPPUB       0x0D
#define REG_INTFA       0x0E
#define REG_INTFB       0x0F
#define REG_GPIOA       0x12
#define REG_GPIOB       0x13
#define REG_OLATA       0x14
#define REG_OLATB       0x15

#define TEST_ADDR       0x20

static const mcp23017_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static mcp23017_t dev;

static void mock_reset(void)
{
    mock_i2c_reset();
}

static mcp23017_err_t init_ok(void)
{
    mock_reset();
    return mcp23017_init(&dev, &mock_i2c, TEST_ADDR);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    /* After init, IODIR should be 0xFF (all input), others 0x00 */
    ASSERT_EQ(mock_regs[REG_IODIRA], 0xFF);
    ASSERT_EQ(mock_regs[REG_IODIRB], 0xFF);
    ASSERT_EQ(mock_regs[REG_GPIOA], 0x00);
    ASSERT_EQ(mock_regs[REG_GPIOB], 0x00);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(mcp23017_init(NULL, &mock_i2c, TEST_ADDR), MCP23017_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ASSERT_EQ(mcp23017_init(&dev, NULL, TEST_ADDR), MCP23017_ERR_PARAM);
}

TEST(test_init_addr_range_low)
{
    mock_reset();
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, 0x1F), MCP23017_ERR_PARAM);
}

TEST(test_init_addr_range_high)
{
    mock_reset();
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, 0x28), MCP23017_ERR_PARAM);
}

TEST(test_init_addr_range_valid_boundary)
{
    mock_reset();
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, 0x27), MCP23017_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* fail on first write */
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, TEST_ADDR), MCP23017_ERR_I2C);
}

/* ---------- direction tests ---------- */

TEST(test_set_direction_port_a)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x0F), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_IODIRA], 0x0F);
    /* Port B unchanged */
    ASSERT_EQ(mock_regs[REG_IODIRB], 0xFF);
}

TEST(test_set_direction_port_b)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_direction(&dev, MCP23017_PORT_B, 0xF0), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_IODIRB], 0xF0);
    /* Port A unchanged */
    ASSERT_EQ(mock_regs[REG_IODIRA], 0xFF);
}

/* ---------- write/read port tests ---------- */

TEST(test_write_port_a)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_write_port(&dev, MCP23017_PORT_A, 0xAA), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPIOA], 0xAA);
}

TEST(test_read_port_b)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_GPIOB] = 0x55;
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_B, &val), MCP23017_OK);
    ASSERT_EQ(val, 0x55);
}

/* ---------- pin-level read/write tests ---------- */

TEST(test_write_pin_sets_bit)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_OLATA] = 0x00;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 3, true), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_OLATA], 0x08);
}

TEST(test_write_pin_clears_bit)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_OLATA] = 0xFF;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 3, false), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_OLATA], 0xF7);
}

TEST(test_write_pin_preserves_other_pins)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_OLATB] = 0xA5;   /* 1010_0101 */
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_B, 1, true), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_OLATB], 0xA7);  /* 1010_0111 — only bit 1 changed */
}

TEST(test_read_pin)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_GPIOA] = 0x42;   /* bit 1 and bit 6 set */
    bool level = false;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 6, &level), MCP23017_OK);
    ASSERT_TRUE(level);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, &level), MCP23017_OK);
    ASSERT_FALSE(level);
}

TEST(test_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    bool level;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 8, true), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 8, &level), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 255, true), MCP23017_ERR_PARAM);
}

/* ---------- pullup tests ---------- */

TEST(test_pullup_config)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_A, 0x0F), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPPUA], 0x0F);
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_B, 0xF0), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPPUB], 0xF0);
}

/* ---------- interrupt tests ---------- */

TEST(test_interrupt_mask)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0x81), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPINTENA], 0x81);
}

TEST(test_interrupt_flag_read)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_INTFB] = 0x24;
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_B, &flags), MCP23017_OK);
    ASSERT_EQ(flags, 0x24);
}

/* ---------- I2C failure propagation ---------- */

TEST(test_i2c_failure_propagation)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    uint8_t val = 0;

    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_set_direction(&dev, MCP23017_PORT_A, 0x00), MCP23017_ERR_I2C);

    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, &val), MCP23017_ERR_I2C);

    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_write_port(&dev, MCP23017_PORT_A, 0xFF), MCP23017_ERR_I2C);
}

/* ---------- NULL param safety ---------- */

TEST(test_null_params)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    uint8_t val;
    bool level;

    ASSERT_EQ(mcp23017_set_direction(NULL, MCP23017_PORT_A, 0), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, NULL), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAM);

    /* suppress unused-variable warnings */
    (void)val;
    (void)level;
}

/* ---------- both ports independent ---------- */

TEST(test_ports_independent)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_write_port(&dev, MCP23017_PORT_A, 0xAA), MCP23017_OK);
    ASSERT_EQ(mcp23017_write_port(&dev, MCP23017_PORT_B, 0x55), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPIOA], 0xAA);
    ASSERT_EQ(mock_regs[REG_GPIOB], 0x55);

    uint8_t a = 0, b = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, &a), MCP23017_OK);
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_B, &b), MCP23017_OK);
    ASSERT_EQ(a, 0xAA);
    ASSERT_EQ(b, 0x55);
}

/* ---------- invalid port (line 99 / 112 / 138 / 179) ---------- */

TEST(test_invalid_port_all_funcs)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mcp23017_port_t bad = (mcp23017_port_t)2;
    uint8_t val;
    bool level;

    ASSERT_EQ(mcp23017_set_direction(&dev, bad, 0xFF), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_set_pullup(&dev, bad, 0xFF), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_port(&dev, bad, 0xFF), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_port(&dev, bad, &val), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_pin(&dev, bad, 0, true), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_pin(&dev, bad, 0, &level), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, bad, 0xFF), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, bad, &val), MCP23017_ERR_PARAM);
}

/* ---------- I2C failure on set_pullup (line 103) ---------- */

TEST(test_i2c_fail_set_pullup)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_A, 0xFF), MCP23017_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_set_pullup(&dev, MCP23017_PORT_B, 0xFF), MCP23017_ERR_I2C);
}

/* ---------- I2C failure on write_pin: read step and write step (lines 145, 153) ---------- */

TEST(test_i2c_fail_write_pin_read_step)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    /* The first I2C operation inside write_pin is a read of OLAT */
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 0, true), MCP23017_ERR_I2C);
}

TEST(test_i2c_fail_write_pin_write_step)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    /* Let the read succeed, then fail on write */
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_B, 0, true), MCP23017_ERR_I2C);
    mock_fail_at = -1;
}

/* ---------- I2C failure on read_pin (line 169) ---------- */

TEST(test_i2c_fail_read_pin)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    bool level;
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, &level), MCP23017_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_B, 0, &level), MCP23017_ERR_I2C);
}

/* ---------- I2C failure on set_interrupt (line 183) ---------- */

TEST(test_i2c_fail_set_interrupt)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_A, 0xFF), MCP23017_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_B, 0xFF), MCP23017_ERR_I2C);
}

/* ---------- I2C failure on read_interrupt_flag (line 197) ---------- */

TEST(test_i2c_fail_read_interrupt_flag)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    uint8_t flags;
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, &flags), MCP23017_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_B, &flags), MCP23017_ERR_I2C);
}

/* ---------- port B variants for read_port (line 127 branch), write_pin port B read step ---------- */

TEST(test_read_port_a)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_GPIOA] = 0xC3;
    uint8_t val = 0;
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, &val), MCP23017_OK);
    ASSERT_EQ(val, 0xC3);
}

/* ---------- interrupt flag port A read (line 195 branch) ---------- */

TEST(test_interrupt_flag_port_a)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    mock_regs[REG_INTFA] = 0x11;
    uint8_t flags = 0;
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, &flags), MCP23017_OK);
    ASSERT_EQ(flags, 0x11);
}

/* ---------- set_interrupt port B (line 181 branch) ---------- */

TEST(test_interrupt_mask_port_b)
{
    ASSERT_EQ(init_ok(), MCP23017_OK);
    ASSERT_EQ(mcp23017_set_interrupt(&dev, MCP23017_PORT_B, 0x42), MCP23017_OK);
    ASSERT_EQ(mock_regs[REG_GPINTENB], 0x42);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(mcp23017_t *d)
{
    uint8_t value = 0;
    bool level = false;
    ASSERT_EQ(mcp23017_set_direction(d, MCP23017_PORT_A, 0), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_set_pullup(d, MCP23017_PORT_A, 0), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_port(d, MCP23017_PORT_A, 0), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_port(d, MCP23017_PORT_A, &value), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_write_pin(d, MCP23017_PORT_A, 0, true), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_pin(d, MCP23017_PORT_A, 0, &level), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_set_interrupt(d, MCP23017_PORT_A, 0), MCP23017_ERR_PARAM);
    ASSERT_EQ(mcp23017_read_interrupt_flag(d, MCP23017_PORT_A, &value),
              MCP23017_ERR_PARAM);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_failed_init)
{
    mcp23017_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(mcp23017_init(&d, NULL, 0x20), MCP23017_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(mcp23017_init(&d, &mock_i2c, 0x28), MCP23017_ERR_PARAM);
    assert_device_refused(&d);

    /* IODIR was never written back, and the bus is fine again */
    mock_fail_at = 0;
    ASSERT_EQ(mcp23017_init(&d, &mock_i2c, 0x20), MCP23017_ERR_I2C);
    mock_reset();
    assert_device_refused(&d);
}

TEST_MAIN("mcp23017")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_addr_range_low);
    RUN(test_init_addr_range_high);
    RUN(test_init_addr_range_valid_boundary);
    RUN(test_init_i2c_fail);
    RUN(test_set_direction_port_a);
    RUN(test_set_direction_port_b);
    RUN(test_write_port_a);
    RUN(test_read_port_b);
    RUN(test_write_pin_sets_bit);
    RUN(test_write_pin_clears_bit);
    RUN(test_write_pin_preserves_other_pins);
    RUN(test_read_pin);
    RUN(test_pin_out_of_range);
    RUN(test_pullup_config);
    RUN(test_interrupt_mask);
    RUN(test_interrupt_flag_read);
    RUN(test_i2c_failure_propagation);
    RUN(test_null_params);
    RUN(test_ports_independent);
    RUN(test_invalid_port_all_funcs);
    RUN(test_i2c_fail_set_pullup);
    RUN(test_i2c_fail_write_pin_read_step);
    RUN(test_i2c_fail_write_pin_write_step);
    RUN(test_i2c_fail_read_pin);
    RUN(test_i2c_fail_set_interrupt);
    RUN(test_i2c_fail_read_interrupt_flag);
    RUN(test_read_port_a);
    RUN(test_interrupt_flag_port_a);
    RUN(test_interrupt_mask_port_b);
    RUN(test_device_refused_after_failed_init);
TEST_MAIN_END()
