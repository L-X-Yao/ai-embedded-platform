// SPDX-License-Identifier: MIT
/*
 * bmi270 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "bmi270.h"

/* TODO: command defines */

static uint8_t spi_cmd(const bmi270_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    uint8_t r = spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
    return r;
}

bmi270_err_t bmi270_init(bmi270_t *dev,
                               const bmi270_spi_t *spi)
{
    if (!dev || !spi) return BMI270_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return BMI270_ERR_PARAM;
    dev->spi = spi;
    /* TODO: read JEDEC ID — e.g. spi_cmd(spi, 0x9F) */
    (void)spi_cmd;
    return BMI270_OK;
}
