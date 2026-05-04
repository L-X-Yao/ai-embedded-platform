// SPDX-License-Identifier: MIT
/*
 * prop_sd_spi.c — property-based tests for SD SPI card driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "sd_spi.h"

/* SD SPI protocol mock: responds to CMD0/CMD8/ACMD41/CMD58 init sequence.
 * For property tests we just ensure no crash with random SPI responses. */

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);
    /* Return 0x01 (idle) for most commands, 0x00 (ready) after a few */
    if (mock_spi_tx_log_count > 20) return 0x00;
    return 0x01;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const sd_spi_hal_t prop_hal = {
    .cs_low   = mock_spi_cs_low,
    .cs_high  = mock_spi_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

PROPERTY(init_no_crash, 5000)
{
    mock_spi_reset();
    sd_spi_t sd;
    sd_spi_init(&sd, &prop_hal);
    /* init may fail (card not responding properly), that is fine */
}

PROPERTY(read_no_crash, 5000)
{
    mock_spi_reset();
    sd_spi_t sd;
    sd.hal = &prop_hal;
    sd.card_type = SD_SPI_TYPE_SDV2_HC;
    sd.block_count = 1024;

    uint8_t buf[512];
    uint32_t block = ARBI_UINT32(seed) % 1024;
    sd_spi_read_block(&sd, block, buf);
}

TEST_MAIN("prop_sd_spi")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
