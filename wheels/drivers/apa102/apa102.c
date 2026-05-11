// SPDX-License-Identifier: MIT
/*
 * apa102 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "apa102.h"

/* TODO: command defines */

static uint8_t spi_cmd(const apa102_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

apa102_err_t apa102_init(apa102_t *dev,
                               const apa102_spi_t *spi)
{
    if (!dev || !spi) return APA102_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return APA102_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return APA102_OK;
}
