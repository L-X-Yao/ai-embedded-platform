// SPDX-License-Identifier: MIT
/*
 * max6675 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "max6675.h"

/* TODO: command defines */

static uint8_t spi_cmd(const max6675_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

max6675_err_t max6675_init(max6675_t *dev,
                               const max6675_spi_t *spi)
{
    if (!dev || !spi) return MAX6675_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return MAX6675_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return MAX6675_OK;
}
