// SPDX-License-Identifier: MIT
/*
 * st7735 — ST7735 128x160 SPI TFT LCD driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * Init sequence: HW reset -> SLPOUT -> COLMOD(0x05) -> MADCTL -> NORON -> DISPON
 * Drawing: set_window (CASET+RASET) then RAMWR + pixel data (big-endian RGB565).
 */

#include "st7735.h"
#include "contract.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static bool send_cmd(const st7735_t *dev, uint8_t cmd)
{
    const st7735_spi_t *spi = dev->spi;
    spi->dc_command(spi->ctx);
    if (!spi->cs_select(spi->ctx)) {
        return false;
    }
    bool ok = spi->transfer(spi->ctx, &cmd, NULL, 1);
    spi->cs_deselect(spi->ctx);
    return ok;
}

static bool send_data(const st7735_t *dev, const uint8_t *data, uint16_t len)
{
    const st7735_spi_t *spi = dev->spi;
    spi->dc_data(spi->ctx);
    if (!spi->cs_select(spi->ctx)) {
        return false;
    }
    bool ok = spi->transfer(spi->ctx, data, NULL, len);
    spi->cs_deselect(spi->ctx);
    return ok;
}

static bool send_cmd_data(const st7735_t *dev, uint8_t cmd,
                          const uint8_t *data, uint16_t len)
{
    if (!send_cmd(dev, cmd)) {
        return false;
    }
    if (len > 0 && data) {
        if (!send_data(dev, data, len)) {
            return false;
        }
    }
    return true;
}

static bool validate_hal(const st7735_spi_t *spi)
{
    return spi->cs_select && spi->cs_deselect && spi->transfer &&
           spi->dc_command && spi->dc_data && spi->reset && spi->delay_ms;
}

/* ---------- public API ---------- */

/* The steps of init that talk to the display */
static st7735_err_t bring_up(const st7735_t *dev)
{
    const st7735_spi_t *spi = dev->spi;

    /* Hardware reset */
    spi->reset(spi->ctx);
    spi->delay_ms(spi->ctx, 120);

    /* Sleep out */
    if (!send_cmd(dev, (uint8_t)ST7735_CMD_SLPOUT)) {
        return ST7735_ERR_SPI;
    }
    spi->delay_ms(spi->ctx, 120);

    /* Color mode: 16-bit RGB565 (0x05) */
    {
        uint8_t colmod = 0x05;
        if (!send_cmd_data(dev, (uint8_t)ST7735_CMD_COLMOD, &colmod, 1)) {
            return ST7735_ERR_SPI;
        }
    }

    /* Memory data access control (MADCTL) */
    {
        uint8_t madctl = 0x00;
        if (!send_cmd_data(dev, (uint8_t)ST7735_CMD_MADCTL, &madctl, 1)) {
            return ST7735_ERR_SPI;
        }
    }

    /* Normal display mode on */
    if (!send_cmd(dev, (uint8_t)ST7735_CMD_NORON)) {
        return ST7735_ERR_SPI;
    }
    spi->delay_ms(spi->ctx, 10);

    /* Display on */
    if (!send_cmd(dev, (uint8_t)ST7735_CMD_DISPON)) {
        return ST7735_ERR_SPI;
    }
    spi->delay_ms(spi->ctx, 10);

    return ST7735_OK;
}

st7735_err_t st7735_init(st7735_t *dev, const st7735_spi_t *spi)
{
    if (!dev) {
        return ST7735_ERR_PARAM;
    }

    /* dev->spi stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi) {
        return ST7735_ERR_PARAM;
    }
    if (!validate_hal(spi)) {
        return ST7735_ERR_PARAM;
    }

    dev->spi = spi;

    st7735_err_t err = bring_up(dev);
    if (err != ST7735_OK) {
        dev->spi = NULL;
        return err;
    }

    return ST7735_OK;
}

st7735_err_t st7735_set_window(st7735_t *dev, uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1)
{
    if (!dev || !dev->spi) {
        return ST7735_ERR_PARAM;
    }
    if (x0 > x1 || y0 > y1) {
        return ST7735_ERR_PARAM;
    }
    if (x1 >= ST7735_WIDTH || y1 >= ST7735_HEIGHT) {
        return ST7735_ERR_PARAM;
    }

    /* Column address set */
    {
        uint8_t caset[4] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFFu),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFFu),
        };
        if (!send_cmd_data(dev, (uint8_t)ST7735_CMD_CASET, caset, 4)) {
            return ST7735_ERR_SPI;
        }
    }

    /* Row address set */
    {
        uint8_t raset[4] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFFu),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFFu),
        };
        if (!send_cmd_data(dev, (uint8_t)ST7735_CMD_RASET, raset, 4)) {
            return ST7735_ERR_SPI;
        }
    }

    return ST7735_OK;
}

st7735_err_t st7735_write_pixels(st7735_t *dev, const uint16_t *pixels,
                                  uint32_t count)
{
    if (!dev || !dev->spi) {
        return ST7735_ERR_PARAM;
    }
    if (!pixels || count == 0) {
        return ST7735_ERR_PARAM;
    }

    /* Send RAMWR command */
    if (!send_cmd(dev, (uint8_t)ST7735_CMD_RAMWR)) {
        return ST7735_ERR_SPI;
    }

    /* Stream pixel data (big-endian RGB565) */
    const st7735_spi_t *spi = dev->spi;
    spi->dc_data(spi->ctx);
    if (!spi->cs_select(spi->ctx)) {
        return ST7735_ERR_SPI;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint8_t buf[2];
        buf[0] = (uint8_t)(pixels[i] >> 8);
        buf[1] = (uint8_t)(pixels[i] & 0xFFu);
        if (!spi->transfer(spi->ctx, buf, NULL, 2)) {
            spi->cs_deselect(spi->ctx);
            return ST7735_ERR_SPI;
        }
    }

    spi->cs_deselect(spi->ctx);
    return ST7735_OK;
}

st7735_err_t st7735_fill_rect(st7735_t *dev, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, uint16_t color)
{
    if (!dev || !dev->spi) {
        return ST7735_ERR_PARAM;
    }
    if (w == 0 || h == 0) {
        return ST7735_ERR_PARAM;
    }
    if ((uint32_t)x + w > ST7735_WIDTH || (uint32_t)y + h > ST7735_HEIGHT) {
        return ST7735_ERR_PARAM;
    }

    st7735_err_t err = st7735_set_window(dev, x, y,
                                          (uint16_t)(x + w - 1),
                                          (uint16_t)(y + h - 1));
    if (err != ST7735_OK) {
        return err;
    }

    /* Send RAMWR command */
    if (!send_cmd(dev, (uint8_t)ST7735_CMD_RAMWR)) {
        return ST7735_ERR_SPI;
    }

    /* Stream fill color */
    const st7735_spi_t *spi = dev->spi;
    spi->dc_data(spi->ctx);
    if (!spi->cs_select(spi->ctx)) {
        return ST7735_ERR_SPI;
    }

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFFu);
    uint32_t total = (uint32_t)w * (uint32_t)h;

    for (uint32_t i = 0; i < total; i++) {
        uint8_t buf[2] = { hi, lo };
        if (!spi->transfer(spi->ctx, buf, NULL, 2)) {
            spi->cs_deselect(spi->ctx);
            return ST7735_ERR_SPI;
        }
    }

    spi->cs_deselect(spi->ctx);
    return ST7735_OK;
}

st7735_err_t st7735_fill_screen(st7735_t *dev, uint16_t color)
{
    return st7735_fill_rect(dev, 0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

st7735_err_t st7735_draw_pixel(st7735_t *dev, uint16_t x, uint16_t y,
                                uint16_t color)
{
    if (!dev || !dev->spi) {
        return ST7735_ERR_PARAM;
    }
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) {
        return ST7735_ERR_PARAM;
    }

    st7735_err_t err = st7735_set_window(dev, x, y, x, y);
    if (err != ST7735_OK) {
        return err;
    }

    uint16_t px = color;
    return st7735_write_pixels(dev, &px, 1);
}
