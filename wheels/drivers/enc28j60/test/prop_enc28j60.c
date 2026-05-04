// SPDX-License-Identifier: MIT
/*
 * prop_enc28j60.c — property-based tests for ENC28J60 Ethernet controller
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "enc28j60.h"

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);
    return 0x00;
}

static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const enc28j60_spi_t prop_spi = {
    .cs_low   = mock_spi_cs_low,
    .cs_high  = mock_spi_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay,
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
