// SPDX-License-Identifier: MIT
/*
 * mcp3008 -- Microchip MCP3008 10-bit 8-channel SPI ADC driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI protocol (mode 0,0):
 *   TX: [0x01] [(SGL/DIFF | CH) << 4] [0x00]
 *   RX: [x]    [xx xx xx B9 B8]       [B7..B0]
 *   Result = ((rx[1] & 0x03) << 8) | rx[2]
 */

#include "mcp3008.h"
#include "contract.h"
#include <string.h>

/* 10-bit ADC maximum value */
#define MCP3008_MAX_RAW  1023u

/* Single-ended mode bit */
#define SGL_BIT  0x08u

mcp3008_err_t mcp3008_init(mcp3008_t *dev, const mcp3008_spi_t *spi)
{
    if (!dev)
    {
        return MCP3008_ERR_PARAM;
    }

    /* dev->spi stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi)
    {
        return MCP3008_ERR_PARAM;
    }

    if (!spi->cs_select || !spi->cs_deselect || !spi->transfer)
    {
        return MCP3008_ERR_PARAM;
    }

    dev->spi = spi;

    /* A chip powered up with CS low ignores us until CS has been high */
    spi->cs_deselect(spi->ctx);

    INVARIANT(dev->spi != NULL);
    return MCP3008_OK;
}

static mcp3008_err_t do_read(const mcp3008_spi_t *spi, uint8_t config_byte,
                             uint16_t *raw)
{
    uint8_t tx[3] = { 0x01, config_byte, 0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    if (!spi->cs_select(spi->ctx))
    {
        return MCP3008_ERR_SPI;
    }

    bool ok = spi->transfer(spi->ctx, tx, rx, 3);
    spi->cs_deselect(spi->ctx);

    if (!ok)
    {
        return MCP3008_ERR_SPI;
    }

    *raw = (uint16_t)(((uint16_t)(rx[1] & 0x03u) << 8) | (uint16_t)rx[2]);
    return MCP3008_OK;
}

mcp3008_err_t mcp3008_read_single(mcp3008_t *dev, uint8_t channel,
                                  uint16_t *raw)
{
    if (!dev || !dev->spi || !raw)
    {
        return MCP3008_ERR_PARAM;
    }

    if (channel >= 8)
    {
        return MCP3008_ERR_PARAM;
    }

    uint8_t config = (uint8_t)((SGL_BIT | channel) << 4);
    return do_read(dev->spi, config, raw);
}

mcp3008_err_t mcp3008_read_diff(mcp3008_t *dev, uint8_t diff_pair,
                                uint16_t *raw)
{
    if (!dev || !dev->spi || !raw)
    {
        return MCP3008_ERR_PARAM;
    }

    if (diff_pair >= 4)
    {
        return MCP3008_ERR_PARAM;
    }

    /* Differential mode: SGL=0, D2:D1 = diff_pair, D0 = 0 (even = IN+) */
    uint8_t config = (uint8_t)(diff_pair << 5);
    return do_read(dev->spi, config, raw);
}

mcp3008_err_t mcp3008_read_voltage(mcp3008_t *dev, uint8_t channel,
                                   float vref, float *voltage)
{
    if (!dev || !dev->spi || !voltage)
    {
        return MCP3008_ERR_PARAM;
    }

    if (vref <= 0.0f)
    {
        return MCP3008_ERR_PARAM;
    }

    uint16_t raw;
    mcp3008_err_t err = mcp3008_read_single(dev, channel, &raw);
    if (err != MCP3008_OK)
    {
        return err;
    }

    *voltage = (float)raw * vref / 1024.0f;

    ENSURE(*voltage == *voltage);  /* NaN guard */
    return MCP3008_OK;
}
