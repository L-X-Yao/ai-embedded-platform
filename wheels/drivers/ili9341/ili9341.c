// SPDX-License-Identifier: MIT
/*
 * ili9341 — ILI9341 SPI TFT display driver (240x320, RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * ILI9341 is an SPI display controller with separate D/C (data/command)
 * and RST pins. Commands sent with DC=low, data with DC=high.
 */

#include "ili9341.h"
#include "contract.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static void write_cmd(const ili9341_t *dev, uint8_t cmd)
{
    const ili9341_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, false);   /* command mode */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
}

static void write_data_byte(const ili9341_t *dev, uint8_t data)
{
    const ili9341_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, true);    /* data mode */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, data);
    spi->cs_high(spi->ctx);
}

static void write_data_buf(const ili9341_t *dev,
                           const uint8_t *buf, size_t len)
{
    const ili9341_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, true);    /* data mode */
    spi->cs_low(spi->ctx);
    for (size_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, buf[i]);
    }
    spi->cs_high(spi->ctx);
}

static void write_cmd_data(const ili9341_t *dev, uint8_t cmd,
                           const uint8_t *data, size_t len)
{
    write_cmd(dev, cmd);
    if (len > 0 && data) {
        write_data_buf(dev, data, len);
    }
}

static bool validate_hal(const ili9341_spi_t *spi)
{
    return spi->cs_low && spi->cs_high && spi->xfer &&
           spi->dc_set && spi->rst_set && spi->delay_ms;
}

static uint8_t madctl_for_rotation(ili9341_rotation_t rot)
{
    /* ILI9341 uses BGR subpixel ordering */
    switch (rot) {
    case ILI9341_ROT_0:
        return ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
    case ILI9341_ROT_90:
        return ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
    case ILI9341_ROT_180:
        return ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR;
    case ILI9341_ROT_270:
        return ILI9341_MADCTL_MX | ILI9341_MADCTL_MY |
               ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
    default:
        return ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
    }
}

static void apply_rotation(ili9341_t *dev, ili9341_rotation_t rot)
{
    dev->rotation = rot;
    switch (rot) {
    case ILI9341_ROT_0:
    case ILI9341_ROT_180:
        dev->cur_width  = ILI9341_WIDTH;
        dev->cur_height = ILI9341_HEIGHT;
        break;
    case ILI9341_ROT_90:
    case ILI9341_ROT_270:
        dev->cur_width  = ILI9341_HEIGHT;
        dev->cur_height = ILI9341_WIDTH;
        break;
    default:
        dev->cur_width  = ILI9341_WIDTH;
        dev->cur_height = ILI9341_HEIGHT;
        break;
    }
}

/* ---------- public API ---------- */

ili9341_err_t ili9341_init(ili9341_t *dev,
                               const ili9341_spi_t *spi)
{
    REQUIRE(dev != NULL);
    REQUIRE(spi != NULL);
    if (!dev) {
        return ILI9341_ERR_PARAM;
    }

    /* dev->spi stays NULL unless init accepts the HAL: the other calls refuse
     * a device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi) {
        return ILI9341_ERR_PARAM;
    }
    if (!validate_hal(spi)) {
        return ILI9341_ERR_PARAM;
    }

    dev->spi = spi;
    apply_rotation(dev, ILI9341_ROT_0);

    /* Hardware reset */
    spi->rst_set(spi->ctx, false);
    spi->delay_ms(spi->ctx, 10);
    spi->rst_set(spi->ctx, true);
    spi->delay_ms(spi->ctx, 120);

    /* Software reset */
    write_cmd(dev, ILI9341_CMD_SWRESET);
    spi->delay_ms(spi->ctx, 150);

    /* Sleep out */
    write_cmd(dev, ILI9341_CMD_SLPOUT);
    spi->delay_ms(spi->ctx, 120);

    /* Pixel format: RGB565 */
    write_cmd(dev, ILI9341_CMD_COLMOD);
    write_data_byte(dev, ILI9341_COLMOD_RGB565);

    /* MADCTL: rotation 0, BGR order */
    write_cmd(dev, ILI9341_CMD_MADCTL);
    write_data_byte(dev, madctl_for_rotation(ILI9341_ROT_0));

    /* Display on */
    write_cmd(dev, ILI9341_CMD_DISPON);
    spi->delay_ms(spi->ctx, 20);

    return ILI9341_OK;
}

ili9341_err_t ili9341_set_rotation(ili9341_t *dev,
                                       ili9341_rotation_t rot)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->spi) {
        return ILI9341_ERR_PARAM;
    }
    if (rot > ILI9341_ROT_270) {
        return ILI9341_ERR_PARAM;
    }

    apply_rotation(dev, rot);

    uint8_t madctl = madctl_for_rotation(rot);
    write_cmd(dev, ILI9341_CMD_MADCTL);
    write_data_byte(dev, madctl);

    return ILI9341_OK;
}

ili9341_err_t ili9341_set_window(ili9341_t *dev,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->spi) {
        return ILI9341_ERR_PARAM;
    }
    REQUIRE(x0 <= x1);
    REQUIRE(y0 <= y1);
    if (x0 > x1 || y0 > y1) {
        return ILI9341_ERR_PARAM;
    }
    if (x1 >= dev->cur_width || y1 >= dev->cur_height) {
        return ILI9341_ERR_PARAM;
    }

    /* Column address set */
    {
        uint8_t caset_data[4] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFFu),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFFu),
        };
        write_cmd_data(dev, ILI9341_CMD_CASET, caset_data, 4);
    }

    /* Row address set */
    {
        uint8_t raset_data[4] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFFu),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFFu),
        };
        write_cmd_data(dev, ILI9341_CMD_RASET, raset_data, 4);
    }

    return ILI9341_OK;
}

ili9341_err_t ili9341_fill_rect(ili9341_t *dev,
                                    uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h,
                                    uint16_t color)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->spi) {
        return ILI9341_ERR_PARAM;
    }
    REQUIRE(w > 0);
    REQUIRE(h > 0);
    if (w == 0 || h == 0) {
        return ILI9341_ERR_PARAM;
    }
    if (x + w > dev->cur_width || y + h > dev->cur_height) {
        return ILI9341_ERR_PARAM;
    }

    ili9341_err_t err = ili9341_set_window(
        dev, x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    if (err != ILI9341_OK) {
        return err;
    }

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFFu);

    write_cmd(dev, ILI9341_CMD_RAMWR);

    const ili9341_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, true);
    spi->cs_low(spi->ctx);
    uint32_t total = (uint32_t)w * (uint32_t)h;
    for (uint32_t i = 0; i < total; i++) {
        spi->xfer(spi->ctx, hi);
        spi->xfer(spi->ctx, lo);
    }
    spi->cs_high(spi->ctx);

    return ILI9341_OK;
}

ili9341_err_t ili9341_draw_pixel(ili9341_t *dev,
                                     uint16_t x, uint16_t y,
                                     uint16_t color)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->spi) {
        return ILI9341_ERR_PARAM;
    }
    if (x >= dev->cur_width || y >= dev->cur_height) {
        return ILI9341_ERR_PARAM;
    }

    ili9341_err_t err = ili9341_set_window(dev, x, y, x, y);
    if (err != ILI9341_OK) {
        return err;
    }

    uint8_t pixel[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFFu)};
    write_cmd_data(dev, ILI9341_CMD_RAMWR, pixel, 2);

    return ILI9341_OK;
}

ili9341_err_t ili9341_fill_screen(ili9341_t *dev, uint16_t color)
{
    REQUIRE(dev != NULL);
    if (!dev || !dev->spi) {
        return ILI9341_ERR_PARAM;
    }
    return ili9341_fill_rect(dev, 0, 0, dev->cur_width, dev->cur_height, color);
}

uint16_t ili9341_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    /* RGB565: RRRRR GGGGGG BBBBB */
    return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                      ((uint16_t)(g & 0xFCu) << 3) |
                      ((uint16_t)b >> 3));
}
