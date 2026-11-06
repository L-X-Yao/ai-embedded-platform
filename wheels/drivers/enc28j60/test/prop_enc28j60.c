// SPDX-License-Identifier: MIT
/*
 * prop_enc28j60.c — property-based tests for ENC28J60 Ethernet controller
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "mock_time.h"
#include "enc28j60.h"

/* Answer ESTAT reads with CLKRDY, or init would stop at its poll and the
 * properties would never reach the rest of it. */
static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    static uint8_t last;
    uint8_t rx = (last == 0x1DU) ? 0x01U : 0x00U;
    (void)ctx;
    last = tx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);
    return rx;
}

static const enc28j60_spi_t prop_spi = {
    .cs_low   = mock_spi_cs_low,
    .cs_high  = mock_spi_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_spi_reset();
    uint8_t mac[6];
    for (int i = 0; i < 6; i++)
        mac[i] = ARBI_UINT8(seed);

    enc28j60_t dev;
    enc28j60_init(&dev, &prop_spi, mac);
}

PROPERTY(ops_no_crash, 5000)
{
    mock_spi_reset();
    uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    enc28j60_t dev;
    enc28j60_init(&dev, &prop_spi, mac);

    (void)enc28j60_has_packet(&dev);
    (void)enc28j60_get_revision(&dev);

    uint8_t frame[64];
    uint16_t len;
    enc28j60_receive(&dev, frame, sizeof(frame), &len);
}

TEST_MAIN("prop_enc28j60")
    RUN(init_no_crash);
    RUN(ops_no_crash);
TEST_MAIN_END()
