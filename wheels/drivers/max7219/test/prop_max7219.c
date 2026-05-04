// SPDX-License-Identifier: MIT
/*
 * prop_max7219.c — property-based tests for MAX7219 LED display driver
 *
 * MAX7219 SPI: transfer() sends 2 bytes (addr, data) per command.
 */

#include "test_harness.h"
#include "property_test.h"
#include "max7219.h"

/* --- SPI mock with transfer() callback --- */

static uint8_t mock_tx_log[64];
static int     mock_tx_count;
static int     mock_fail_next;

static bool mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                          uint16_t len)
{
    (void)ctx;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++) {
        if (mock_tx_count < (int)sizeof(mock_tx_log))
            mock_tx_log[mock_tx_count++] = tx[i];
        if (rx) rx[i] = 0x00;
    }
    return true;
}

static int mock_cs_state = 1;
static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static const max7219_spi_t prop_spi = {
    .transfer = mock_transfer,
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    mock_tx_count = 0;
    mock_fail_next = 0;
    mock_cs_state = 1;
    memset(mock_tx_log, 0, sizeof(mock_tx_log));
}

PROPERTY(init_no_crash, 10000)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &prop_spi);
}

PROPERTY(ops_no_crash, 5000)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &prop_spi);

    max7219_set_intensity(&dev, ARBI_UINT8(seed) % 16);
    max7219_set_scan_limit(&dev, (uint8_t)(1 + ARBI_UINT8(seed) % 8));

    uint8_t digit = ARBI_UINT8(seed) % 8;
    max7219_set_digit(&dev, digit, ARBI_UINT8(seed));
    max7219_clear(&dev);
    max7219_shutdown(&dev, ARBI_UINT8(seed) & 1);
}

TEST_MAIN("prop_max7219")
    RUN(init_no_crash);
    RUN(ops_no_crash);
TEST_MAIN_END()
