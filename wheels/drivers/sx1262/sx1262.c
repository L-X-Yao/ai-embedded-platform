// SPDX-License-Identifier: MIT
/*
 * sx1262 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "sx1262.h"

/* TODO: command defines */

static uint8_t spi_cmd(const sx1262_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

sx1262_err_t sx1262_init(sx1262_t *dev,
                               const sx1262_spi_t *spi)
{
    if (!dev || !spi) return SX1262_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return SX1262_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return SX1262_OK;
}
