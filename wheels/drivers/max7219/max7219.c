// SPDX-License-Identifier: MIT
/*
 * max7219 — Maxim MAX7219 LED matrix/7-segment display driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */
#include "max7219.h"
#include <stddef.h>

#define REG_NOOP         0x00
#define REG_DIGIT0       0x01
#define REG_DECODE_MODE  0x09
#define REG_INTENSITY    0x0A
#define REG_SCAN_LIMIT   0x0B
#define REG_SHUTDOWN     0x0C
#define REG_DISPLAY_TEST 0x0F

static max7219_err_t write_reg(max7219_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {reg, val};
    dev->spi.cs_low(dev->spi.ctx);
    bool ok = dev->spi.transfer(dev->spi.ctx, tx, NULL, 2);
    dev->spi.cs_high(dev->spi.ctx);
    return ok ? MAX7219_OK : MAX7219_ERR_SPI;
}

max7219_err_t max7219_init(max7219_t *dev, const max7219_spi_t *spi)
{
    if (!dev || !spi)
        return MAX7219_ERR_PARAM;
    if (!spi->transfer || !spi->cs_low || !spi->cs_high)
        return MAX7219_ERR_PARAM;
    dev->spi = *spi;

    max7219_err_t err;
    err = write_reg(dev, REG_DISPLAY_TEST, 0x00);
    if (err != MAX7219_OK) return err;
    err = write_reg(dev, REG_SCAN_LIMIT, 0x07);
    if (err != MAX7219_OK) return err;
    err = write_reg(dev, REG_DECODE_MODE, 0x00);
    if (err != MAX7219_OK) return err;
    err = write_reg(dev, REG_INTENSITY, 0x08);
    if (err != MAX7219_OK) return err;
    err = write_reg(dev, REG_SHUTDOWN, 0x01);
    if (err != MAX7219_OK) return err;

    return max7219_clear(dev);
}

max7219_err_t max7219_set_intensity(max7219_t *dev, uint8_t level)
{
    if (!dev) return MAX7219_ERR_PARAM;
    if (level > 0x0F) level = 0x0F;
    return write_reg(dev, REG_INTENSITY, level);
}

max7219_err_t max7219_set_scan_limit(max7219_t *dev, uint8_t digits)
{
    if (!dev) return MAX7219_ERR_PARAM;
    if (digits == 0 || digits > 8) return MAX7219_ERR_PARAM;
    return write_reg(dev, REG_SCAN_LIMIT, digits - 1);
}

max7219_err_t max7219_shutdown(max7219_t *dev, bool shutdown)
{
    if (!dev) return MAX7219_ERR_PARAM;
    return write_reg(dev, REG_SHUTDOWN, shutdown ? 0x00 : 0x01);
}

max7219_err_t max7219_set_decode_mode(max7219_t *dev, uint8_t mask)
{
    if (!dev) return MAX7219_ERR_PARAM;
    return write_reg(dev, REG_DECODE_MODE, mask);
}

max7219_err_t max7219_set_digit(max7219_t *dev, uint8_t digit, uint8_t value)
{
    if (!dev) return MAX7219_ERR_PARAM;
    if (digit > 7) return MAX7219_ERR_PARAM;
    return write_reg(dev, REG_DIGIT0 + digit, value);
}

max7219_err_t max7219_clear(max7219_t *dev)
{
    if (!dev) return MAX7219_ERR_PARAM;
    for (uint8_t i = 0; i < 8; i++) {
        max7219_err_t err = write_reg(dev, REG_DIGIT0 + i, 0x00);
        if (err != MAX7219_OK) return err;
    }
    return MAX7219_OK;
}

max7219_err_t max7219_display_test(max7219_t *dev, bool enable)
{
    if (!dev) return MAX7219_ERR_PARAM;
    return write_reg(dev, REG_DISPLAY_TEST, enable ? 0x01 : 0x00);
}
