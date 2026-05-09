// SPDX-License-Identifier: MIT
/*
 * ili9341 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "ili9341.h"

/* TODO: command defines */

static uint8_t spi_cmd(const ili9341_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

ili9341_err_t ili9341_init(ili9341_t *dev,
                               const ili9341_spi_t *spi)
{
    if (!dev || !spi) return ILI9341_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return ILI9341_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return ILI9341_OK;
}
