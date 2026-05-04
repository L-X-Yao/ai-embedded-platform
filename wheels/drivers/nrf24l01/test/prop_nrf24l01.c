// SPDX-License-Identifier: MIT
/*
 * prop_nrf24l01.c — property-based tests for nRF24L01+ radio
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "nrf24l01.h"

/* nRF24 SPI: register-based. Byte 0 = cmd, subsequent = data.
 * Status is returned on every first byte. */

static uint8_t nrf_regs[64];

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);

    int phase = mock_spi_tx_log_count - 1;
    if (phase == 0) {
        /* Command byte: return STATUS register */
        uint8_t cmd = tx & 0x1F;
        if ((tx & 0xE0) == 0x00) {
            /* Read register: cmd = reg addr */
            (void)cmd;
        }
        return nrf_regs[0x07]; /* STATUS */
    }
    /* Subsequent: return register data */
    uint8_t cmd0 = mock_spi_tx_log[0];
    if ((cmd0 & 0xE0) == 0x00) {
        uint8_t reg = (uint8_t)((cmd0 & 0x1F) + (uint8_t)(phase - 1));
        return (reg < sizeof(nrf_regs)) ? nrf_regs[reg] : 0xFF;
    }
    return 0x00;
}

static void mock_ce_low(void *ctx)  { (void)ctx; }
static void mock_ce_high(void *ctx) { (void)ctx; }
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const nrf24_spi_t prop_spi = {
    .cs_low   = mock_spi_cs_low,
    .cs_high  = mock_spi_cs_high,
    .xfer     = mock_xfer,
    .ce_low   = mock_ce_low,
    .ce_high  = mock_ce_high,
    .delay_us = mock_delay_us,
    .ctx      = NULL,
};

static void full_reset(void)
{
    mock_spi_reset();
    memset(nrf_regs, 0, sizeof(nrf_regs));
    nrf_regs[0x07] = 0x0E; /* STATUS: RX_DR=0, TX_DS=0, MAX_RT=0 */
}

PROPERTY(init_no_crash, 10000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(nrf_regs); i++)
        nrf_regs[i] = ARBI_UINT8(seed);

    nrf24_t dev;
    uint8_t ch = ARBI_UINT8(seed) % 126;
    uint8_t ps = (uint8_t)(1 + ARBI_UINT8(seed) % 32);
    nrf24_init(&dev, &prop_spi, ch, ps);
}

PROPERTY(ops_no_crash, 5000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(nrf_regs); i++)
        nrf_regs[i] = ARBI_UINT8(seed);

    nrf24_t dev;
    nrf24_init(&dev, &prop_spi, 76, 16);

    nrf24_set_mode(&dev, (nrf24_mode_t)(ARBI_UINT8(seed) & 1));
    nrf24_set_data_rate(&dev, (nrf24_data_rate_t)(ARBI_UINT8(seed) % 3));
    nrf24_set_power(&dev, (nrf24_power_t)(ARBI_UINT8(seed) % 4));
    nrf24_flush_tx(&dev);
    nrf24_flush_rx(&dev);
    (void)nrf24_read_status(&dev);
}

TEST_MAIN("prop_nrf24l01")
    RUN(init_no_crash);
    RUN(ops_no_crash);
TEST_MAIN_END()
