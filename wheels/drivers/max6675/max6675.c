// SPDX-License-Identifier: MIT
/*
 * max6675 — Maxim MAX6675 K-type thermocouple-to-digital converter
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */
#include "max6675.h"
#include <string.h>

#define MAX6675_OPEN_BIT  (1u << 2)
#define MAX6675_TEMP_MASK 0x7FF8u
#define MAX6675_TEMP_SHIFT 3
#define MAX6675_RESOLUTION 0.25f

static uint16_t spi_read16(const max6675_spi_t *spi)
{
    spi->cs_low(spi->ctx);
    uint8_t msb = spi->xfer(spi->ctx, 0x00);
    uint8_t lsb = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return (uint16_t)((uint16_t)msb << 8 | lsb);
}

max6675_err_t max6675_init(max6675_t *dev, const max6675_spi_t *spi)
{
    if (!dev)
        return MAX6675_ERR_PARAM;

    /* dev->spi stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi)
        return MAX6675_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer)
        return MAX6675_ERR_PARAM;
    dev->spi = spi;
    return MAX6675_OK;
}

max6675_err_t max6675_read_raw(max6675_t *dev, uint16_t *raw)
{
    if (!dev || !dev->spi || !raw)
        return MAX6675_ERR_PARAM;
    *raw = spi_read16(dev->spi);
    return MAX6675_OK;
}

max6675_err_t max6675_read(max6675_t *dev, float *temp_c)
{
    if (!dev || !dev->spi || !temp_c)
        return MAX6675_ERR_PARAM;

    uint16_t raw = spi_read16(dev->spi);

    if (raw & MAX6675_OPEN_BIT)
        return MAX6675_ERR_OPEN;

    uint16_t counts = (raw & MAX6675_TEMP_MASK) >> MAX6675_TEMP_SHIFT;
    *temp_c = (float)counts * MAX6675_RESOLUTION;
    return MAX6675_OK;
}
