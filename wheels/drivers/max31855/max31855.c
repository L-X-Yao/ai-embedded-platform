// SPDX-License-Identifier: MIT
/*
 * max31855 — MAX31855 SPI thermocouple-to-digital converter driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Datasheet: Maxim MAX31855 — 32-bit read-only SPI frame.
 *   Bits [31:18] = thermocouple temp (14-bit signed, 0.25 C/LSB)
 *   Bit  [16]    = fault flag
 *   Bits [15:4]  = internal temp (12-bit signed, 0.0625 C/LSB)
 *   Bit  [2]     = SCV (short to VCC)
 *   Bit  [1]     = SCG (short to GND)
 *   Bit  [0]     = OC  (open circuit)
 */

#include "max31855.h"

/* ---------- internal helpers ---------- */

static uint32_t spi_read32(const max31855_spi_t *spi)
{
    uint32_t raw = 0;
    spi->cs_low(spi->ctx);
    raw  = (uint32_t)spi->xfer(spi->ctx, 0x00) << 24;
    raw |= (uint32_t)spi->xfer(spi->ctx, 0x00) << 16;
    raw |= (uint32_t)spi->xfer(spi->ctx, 0x00) << 8;
    raw |= (uint32_t)spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return raw;
}

static max31855_err_t check_faults(uint32_t raw)
{
    if (raw & 0x01U) return MAX31855_ERR_OPEN;
    if (raw & 0x02U) return MAX31855_ERR_SHORT_GND;
    if (raw & 0x04U) return MAX31855_ERR_SHORT_VCC;
    return MAX31855_OK;
}

static float decode_thermocouple(uint32_t raw)
{
    int32_t tc = (int32_t)(raw >> 18);
    if (tc & 0x2000) tc |= (int32_t)0xFFFFC000;   /* sign-extend 14-bit */
    return (float)tc * 0.25f;
}

static float decode_internal(uint32_t raw)
{
    int32_t it = (int32_t)((raw >> 4) & 0x0FFFU);
    if (it & 0x0800) it |= (int32_t)0xFFFFF000;    /* sign-extend 12-bit */
    return (float)it * 0.0625f;
}

/* ---------- public API ---------- */

max31855_err_t max31855_init(max31855_t *dev, const max31855_spi_t *spi)
{
    if (!dev || !spi)           return MAX31855_ERR_PARAM;
    if (!spi->cs_low ||
        !spi->cs_high ||
        !spi->xfer)            return MAX31855_ERR_SPI;
    dev->spi = spi;
    return MAX31855_OK;
}

max31855_err_t max31855_read(max31855_t *dev,
                             float *thermocouple_c, float *internal_c)
{
    if (!dev || !dev->spi)     return MAX31855_ERR_PARAM;

    uint32_t raw = spi_read32(dev->spi);

    if (raw & (1U << 16)) {
        return check_faults(raw);
    }

    if (thermocouple_c) *thermocouple_c = decode_thermocouple(raw);
    if (internal_c)     *internal_c     = decode_internal(raw);
    return MAX31855_OK;
}

max31855_err_t max31855_read_raw(max31855_t *dev, uint32_t *raw)
{
    if (!dev || !dev->spi || !raw) return MAX31855_ERR_PARAM;
    *raw = spi_read32(dev->spi);
    return MAX31855_OK;
}
