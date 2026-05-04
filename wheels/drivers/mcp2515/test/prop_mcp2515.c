// SPDX-License-Identifier: MIT
/*
 * prop_mcp2515.c — property-based tests for MCP2515 CAN controller
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "mcp2515.h"

/* MCP2515 SPI: instruction byte + address/data bytes.
 * Return 0x00 for most reads to simulate idle chip. */

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);
    /* For READ_STATUS (0xA0), return 0x00 */
    /* For READ (0x03), return 0x00 (all regs) */
    /* For CANSTAT mode bits: return requested mode to avoid timeout */
    if (mock_spi_tx_log_count >= 2 && mock_spi_tx_log[0] == 0x03) {
        uint8_t addr = mock_spi_tx_log[1];
        if (addr == 0x0E) return 0x00; /* CANSTAT = normal mode */
    }
    return 0x00;
}

static const mcp2515_spi_t prop_spi = {
    .cs_low  = mock_spi_cs_low,
    .cs_high = mock_spi_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

PROPERTY(init_no_crash, 5000)
{
    mock_spi_reset();
    mcp2515_t dev;
    mcp2515_init(&dev, &prop_spi,
                 (mcp2515_speed_t)(ARBI_UINT8(seed) % 4));
}

PROPERTY(ops_no_crash, 5000)
{
    mock_spi_reset();
    mcp2515_t dev;
    mcp2515_init(&dev, &prop_spi, MCP2515_SPEED_250KBPS);

    (void)mcp2515_has_message(&dev);
    (void)mcp2515_read_status(&dev);
    (void)mcp2515_read_errors(&dev);

    mcp2515_msg_t msg;
    mcp2515_receive(&dev, &msg);
}

TEST_MAIN("prop_mcp2515")
    RUN(init_no_crash);
    RUN(ops_no_crash);
TEST_MAIN_END()
