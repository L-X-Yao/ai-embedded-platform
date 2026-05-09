// SPDX-License-Identifier: MIT
/*
 * rc522 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "rc522.h"

/* TODO: command defines */

static uint8_t spi_cmd(const rc522_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

rc522_err_t rc522_init(rc522_t *dev,
                               const rc522_spi_t *spi)
{
    if (!dev || !spi) return RC522_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return RC522_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return RC522_OK;
}
