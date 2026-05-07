// SPDX-License-Identifier: MIT
/*
 * gc9a01 — GC9A01 240x240 round TFT LCD SPI driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "gc9a01.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static void write_cmd(const gc9a01_t *dev, uint8_t cmd)
{
    const gc9a01_spi_t *spi = dev->spi;
    spi->dc_cmd(spi->ctx);          /* command mode */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
}

static void write_data_buf(const gc9a01_t *dev,
                           const uint8_t *buf, size_t len)
{
    const gc9a01_spi_t *spi = dev->spi;
    spi->dc_data(spi->ctx);         /* data mode */
    spi->cs_low(spi->ctx);
    for (size_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, buf[i]);
    }
    spi->cs_high(spi->ctx);
}

static void write_cmd_data(const gc9a01_t *dev, uint8_t cmd,
                           const uint8_t *data, size_t len)
{
    write_cmd(dev, cmd);
    if (len > 0 && data) {
        write_data_buf(dev, data, len);
    }
}

static bool validate_hal(const gc9a01_spi_t *spi)
{
    return spi->cs_low && spi->cs_high && spi->xfer &&
           spi->dc_cmd && spi->dc_data &&
           spi->rst_low && spi->rst_high && spi->delay_ms;
}

static uint8_t madctl_for_rotation(uint8_t rot)
{
    switch (rot) {
    case 0:
        return 0x00;
    case 1:
        return GC9A01_MADCTL_MV | GC9A01_MADCTL_MY;
    case 2:
        return GC9A01_MADCTL_MX | GC9A01_MADCTL_MY;
    case 3:
        return GC9A01_MADCTL_MV | GC9A01_MADCTL_MX;
    default:
        return 0x00;
    }
}

/* ---------- public API ---------- */

gc9a01_err_t gc9a01_init(gc9a01_t *dev, const gc9a01_spi_t *spi)
{
    if (!dev || !spi) {
        return GC9A01_ERR_PARAM;
    }
    if (!validate_hal(spi)) {
        return GC9A01_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->spi      = spi;
    dev->rotation = 0;
    dev->width    = GC9A01_WIDTH;
    dev->height   = GC9A01_HEIGHT;

    /* hardware reset */
    spi->rst_low(spi->ctx);
    spi->delay_ms(spi->ctx, 10);
    spi->rst_high(spi->ctx);
    spi->delay_ms(spi->ctx, 120);

    /* software reset */
    write_cmd(dev, GC9A01_CMD_SWRESET);
    spi->delay_ms(spi->ctx, 120);

    /* sleep out */
    write_cmd(dev, GC9A01_CMD_SLPOUT);
    spi->delay_ms(spi->ctx, 120);

    /* color mode: RGB565 */
    {
        uint8_t colmod = GC9A01_COLMOD_RGB565;
        write_cmd_data(dev, GC9A01_CMD_COLMOD, &colmod, 1);
    }

    /* MADCTL: rotation 0 */
    {
        uint8_t madctl = madctl_for_rotation(0);
        write_cmd_data(dev, GC9A01_CMD_MADCTL, &madctl, 1);
    }

    /* display inversion on (GC9A01 typically needs this for correct colors) */
    write_cmd(dev, GC9A01_CMD_INVON);

    /* display on */
    write_cmd(dev, GC9A01_CMD_DISPON);
    spi->delay_ms(spi->ctx, 20);

    return GC9A01_OK;
}

gc9a01_err_t gc9a01_set_window(gc9a01_t *dev,
                               uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }
    if (x0 > x1 || y0 > y1) {
        return GC9A01_ERR_PARAM;
    }
    if (x1 >= dev->width || y1 >= dev->height) {
        return GC9A01_ERR_PARAM;
    }

    /* column address set */
    {
        uint8_t caset_data[4] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
        };
        write_cmd_data(dev, GC9A01_CMD_CASET, caset_data, 4);
    }

    /* row address set */
    {
        uint8_t raset_data[4] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
        };
        write_cmd_data(dev, GC9A01_CMD_RASET, raset_data, 4);
    }

    return GC9A01_OK;
}

gc9a01_err_t gc9a01_write_pixels(gc9a01_t *dev,
                                 const uint16_t *pixels, uint32_t count)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }
    if (!pixels || count == 0) {
        return GC9A01_ERR_PARAM;
    }

    const gc9a01_spi_t *spi = dev->spi;

    /* send RAMWR command */
    spi->dc_cmd(spi->ctx);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, GC9A01_CMD_RAMWR);
    spi->cs_high(spi->ctx);

    /* send pixel data in data mode */
    spi->dc_data(spi->ctx);
    spi->cs_low(spi->ctx);
    for (uint32_t i = 0; i < count; i++) {
        spi->xfer(spi->ctx, (uint8_t)(pixels[i] >> 8));
        spi->xfer(spi->ctx, (uint8_t)(pixels[i] & 0xFF));
    }
    spi->cs_high(spi->ctx);

    return GC9A01_OK;
}

gc9a01_err_t gc9a01_fill(gc9a01_t *dev, uint16_t color)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }

    gc9a01_err_t err = gc9a01_set_window(
        dev, 0, 0, (uint16_t)(dev->width - 1), (uint16_t)(dev->height - 1));
    if (err != GC9A01_OK) {
        return err;
    }

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    const gc9a01_spi_t *spi = dev->spi;

    /* send RAMWR command */
    spi->dc_cmd(spi->ctx);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, GC9A01_CMD_RAMWR);
    spi->cs_high(spi->ctx);

    /* send pixel data */
    spi->dc_data(spi->ctx);
    spi->cs_low(spi->ctx);
    uint32_t total = (uint32_t)dev->width * (uint32_t)dev->height;
    for (uint32_t i = 0; i < total; i++) {
        spi->xfer(spi->ctx, hi);
        spi->xfer(spi->ctx, lo);
    }
    spi->cs_high(spi->ctx);

    return GC9A01_OK;
}

gc9a01_err_t gc9a01_set_rotation(gc9a01_t *dev, uint8_t rotation)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }
    if (rotation > 3) {
        return GC9A01_ERR_PARAM;
    }

    dev->rotation = rotation;
    /* GC9A01 is 240x240 so width/height do not change with rotation */
    dev->width    = GC9A01_WIDTH;
    dev->height   = GC9A01_HEIGHT;

    uint8_t madctl = madctl_for_rotation(rotation);
    write_cmd_data(dev, GC9A01_CMD_MADCTL, &madctl, 1);

    return GC9A01_OK;
}

gc9a01_err_t gc9a01_sleep(gc9a01_t *dev, bool enable)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }
    write_cmd(dev, enable ? GC9A01_CMD_SLPIN : GC9A01_CMD_SLPOUT);
    if (!enable) {
        /* datasheet: need 120ms delay after sleep out */
        dev->spi->delay_ms(dev->spi->ctx, 120);
    }
    return GC9A01_OK;
}

gc9a01_err_t gc9a01_display(gc9a01_t *dev, bool on)
{
    if (!dev || !dev->spi) {
        return GC9A01_ERR_PARAM;
    }
    write_cmd(dev, on ? GC9A01_CMD_DISPON : GC9A01_CMD_DISPOFF);
    return GC9A01_OK;
}

uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                      ((uint16_t)(g & 0xFC) << 3) |
                      ((uint16_t)b >> 3));
}
