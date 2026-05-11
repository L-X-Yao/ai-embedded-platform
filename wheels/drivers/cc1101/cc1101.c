// SPDX-License-Identifier: MIT
/*
 * cc1101 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "cc1101.h"

/* TODO: command defines */

static uint8_t spi_cmd(const cc1101_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

cc1101_err_t cc1101_init(cc1101_t *dev,
                               const cc1101_spi_t *spi)
{
    if (!dev || !spi) return CC1101_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return CC1101_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return CC1101_OK;
}
