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
    ASSERT_EQ(mcp23017_init(NULL, &mock_i2c, TEST_ADDR), MCP23017_ERR_PARAMS);
}

TEST(test_init_null_i2c)
{
    ASSERT_EQ(mcp23017_init(&dev, NULL, TEST_ADDR), MCP23017_ERR_PARAMS);
}

TEST(test_init_addr_range_low)
{
    mock_reset();
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, 0x1F), MCP23017_ERR_PARAMS);
}

TEST(test_init_addr_range_high)
{
    mock_reset();
    ASSERT_EQ(mcp23017_init(&dev, &mock_i2c, 0x28), MCP23017_ERR_PARAMS);
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
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 8, true), MCP23017_ERR_PARAMS);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 8, &level), MCP23017_ERR_PARAMS);
    ASSERT_EQ(mcp23017_write_pin(&dev, MCP23017_PORT_A, 255, true), MCP23017_ERR_PARAMS);
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

    ASSERT_EQ(mcp23017_set_direction(NULL, MCP23017_PORT_A, 0), MCP23017_ERR_PARAMS);
    ASSERT_EQ(mcp23017_read_port(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAMS);
    ASSERT_EQ(mcp23017_read_pin(&dev, MCP23017_PORT_A, 0, NULL), MCP23017_ERR_PARAMS);
    ASSERT_EQ(mcp23017_read_interrupt_flag(&dev, MCP23017_PORT_A, NULL), MCP23017_ERR_PARAMS);

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
TEST_MAIN_END()
