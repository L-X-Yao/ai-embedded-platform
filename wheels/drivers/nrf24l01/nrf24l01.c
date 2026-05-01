/*
 * nrf24l01 — TODO: one-line description
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#include "nrf24l01.h"

/* TODO: command defines */

static uint8_t spi_cmd(const nrf24l01_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

nrf24l01_err_t nrf24l01_init(const nrf24l01_spi_t *spi)
{
    if (!spi) return NRF24L01_ERR_SPI;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return NRF24L01_OK;
}
