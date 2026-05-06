// SPDX-License-Identifier: MIT
/*
 * lcd_st7789 — ST7789 240x320 TFT LCD SPI driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "lcd_st7789.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static void write_cmd(const lcd_st7789_t *dev, uint8_t cmd)
{
    const lcd_st7789_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, false);   /* command mode */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
}

static void write_data_buf(const lcd_st7789_t *dev,
                           const uint8_t *buf, size_t len)
{
    const lcd_st7789_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, true);    /* data mode */
    spi->cs_low(spi->ctx);
    for (size_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, buf[i]);
    }
    spi->cs_high(spi->ctx);
}

static void write_cmd_data(const lcd_st7789_t *dev, uint8_t cmd,
                           const uint8_t *data, size_t len)
{
    write_cmd(dev, cmd);
    if (len > 0 && data) {
        write_data_buf(dev, data, len);
    }
}

static bool validate_hal(const lcd_st7789_spi_t *spi)
{
    return spi->cs_low && spi->cs_high && spi->xfer &&
           spi->dc_set && spi->rst_set && spi->delay_ms;
}

static uint8_t madctl_for_rotation(lcd_st7789_rotation_t rot)
{
    switch (rot) {
    case LCD_ST7789_ROT_0:
        return LCD_ST7789_MADCTL_RGB;
    case LCD_ST7789_ROT_90:
        return LCD_ST7789_MADCTL_MV | LCD_ST7789_MADCTL_MY | LCD_ST7789_MADCTL_RGB;
    case LCD_ST7789_ROT_180:
        return LCD_ST7789_MADCTL_MX | LCD_ST7789_MADCTL_MY | LCD_ST7789_MADCTL_RGB;
    case LCD_ST7789_ROT_270:
        return LCD_ST7789_MADCTL_MV | LCD_ST7789_MADCTL_MX | LCD_ST7789_MADCTL_RGB;
    default:
        return LCD_ST7789_MADCTL_RGB;
    }
}

static void apply_rotation(lcd_st7789_t *dev, lcd_st7789_rotation_t rot)
{
    dev->rotation = rot;
    switch (rot) {
    case LCD_ST7789_ROT_0:
        dev->cur_width  = dev->cfg.width;
        dev->cur_height = dev->cfg.height;
        dev->col_offset = dev->cfg.col_offset;
        dev->row_offset = dev->cfg.row_offset;
        break;
    case LCD_ST7789_ROT_90:
        dev->cur_width  = dev->cfg.height;
        dev->cur_height = dev->cfg.width;
        dev->col_offset = dev->cfg.row_offset;
        dev->row_offset = dev->cfg.col_offset;
        break;
    case LCD_ST7789_ROT_180:
        dev->cur_width  = dev->cfg.width;
        dev->cur_height = dev->cfg.height;
        dev->col_offset = dev->cfg.col_offset;
        dev->row_offset = dev->cfg.row_offset;
        break;
    case LCD_ST7789_ROT_270:
        dev->cur_width  = dev->cfg.height;
        dev->cur_height = dev->cfg.width;
        dev->col_offset = dev->cfg.row_offset;
        dev->row_offset = dev->cfg.col_offset;
        break;
    default:
        dev->cur_width  = dev->cfg.width;
        dev->cur_height = dev->cfg.height;
        dev->col_offset = dev->cfg.col_offset;
        dev->row_offset = dev->cfg.row_offset;
        break;
    }
}

/* ---------- public API ---------- */

lcd_st7789_err_t lcd_st7789_init(lcd_st7789_t *dev,
                                 const lcd_st7789_spi_t *spi,
                                 const lcd_st7789_cfg_t *cfg)
{
    if (!dev || !spi || !cfg) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (!validate_hal(spi)) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (cfg->width == 0 || cfg->height == 0) {
        return LCD_ST7789_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->spi = spi;
    dev->cfg = *cfg;
    apply_rotation(dev, LCD_ST7789_ROT_0);

    /* hardware reset */
    spi->rst_set(spi->ctx, false);
    spi->delay_ms(spi->ctx, 10);
    spi->rst_set(spi->ctx, true);
    spi->delay_ms(spi->ctx, 120);

    /* software reset */
    write_cmd(dev, LCD_ST7789_CMD_SWRESET);
    spi->delay_ms(spi->ctx, 150);

    /* sleep out */
    write_cmd(dev, LCD_ST7789_CMD_SLPOUT);
    spi->delay_ms(spi->ctx, 120);

    /* color mode: RGB565 */
    {
        uint8_t colmod = LCD_ST7789_COLMOD_RGB565;
        write_cmd_data(dev, LCD_ST7789_CMD_COLMOD, &colmod, 1);
    }

    /* MADCTL: rotation 0, RGB order */
    {
        uint8_t madctl = madctl_for_rotation(LCD_ST7789_ROT_0);
        write_cmd_data(dev, LCD_ST7789_CMD_MADCTL, &madctl, 1);
    }

    /* display inversion on (ST7789 typically needs this for correct colors) */
    write_cmd(dev, LCD_ST7789_CMD_INVON);

    /* display on */
    write_cmd(dev, LCD_ST7789_CMD_DISPON);
    spi->delay_ms(spi->ctx, 20);

    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_set_window(lcd_st7789_t *dev,
                                       uint16_t x0, uint16_t y0,
                                       uint16_t x1, uint16_t y1)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (x0 > x1 || y0 > y1) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (x1 >= dev->cur_width || y1 >= dev->cur_height) {
        return LCD_ST7789_ERR_PARAM;
    }

    uint16_t xa0 = x0 + dev->col_offset;
    uint16_t xa1 = x1 + dev->col_offset;
    uint16_t ya0 = y0 + dev->row_offset;
    uint16_t ya1 = y1 + dev->row_offset;

    /* column address set */
    {
        uint8_t caset_data[4] = {
            (uint8_t)(xa0 >> 8), (uint8_t)(xa0 & 0xFF),
            (uint8_t)(xa1 >> 8), (uint8_t)(xa1 & 0xFF),
        };
        write_cmd_data(dev, LCD_ST7789_CMD_CASET, caset_data, 4);
    }

    /* row address set */
    {
        uint8_t raset_data[4] = {
            (uint8_t)(ya0 >> 8), (uint8_t)(ya0 & 0xFF),
            (uint8_t)(ya1 >> 8), (uint8_t)(ya1 & 0xFF),
        };
        write_cmd_data(dev, LCD_ST7789_CMD_RASET, raset_data, 4);
    }

    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_write_pixels(lcd_st7789_t *dev,
                                         const uint8_t *data,
                                         size_t len)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (!data || len == 0) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (len % 2 != 0) {
        return LCD_ST7789_ERR_PARAM;
    }

    write_cmd_data(dev, LCD_ST7789_CMD_RAMWR, data, len);
    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_fill_rect(lcd_st7789_t *dev,
                                      uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h,
                                      uint16_t color)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (w == 0 || h == 0) {
        return LCD_ST7789_ERR_PARAM;
    }
    /* check overflow and bounds */
    if (x + w > dev->cur_width || y + h > dev->cur_height) {
        return LCD_ST7789_ERR_PARAM;
    }

    lcd_st7789_err_t err = lcd_st7789_set_window(
        dev, x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    if (err != LCD_ST7789_OK) {
        return err;
    }

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    write_cmd(dev, LCD_ST7789_CMD_RAMWR);

    const lcd_st7789_spi_t *spi = dev->spi;
    spi->dc_set(spi->ctx, true);
    spi->cs_low(spi->ctx);
    uint32_t total = (uint32_t)w * (uint32_t)h;
    for (uint32_t i = 0; i < total; i++) {
        spi->xfer(spi->ctx, hi);
        spi->xfer(spi->ctx, lo);
    }
    spi->cs_high(spi->ctx);

    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_display_on(lcd_st7789_t *dev)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    write_cmd(dev, LCD_ST7789_CMD_DISPON);
    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_display_off(lcd_st7789_t *dev)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    write_cmd(dev, LCD_ST7789_CMD_DISPOFF);
    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_set_rotation(lcd_st7789_t *dev,
                                         lcd_st7789_rotation_t rot)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    if (rot > LCD_ST7789_ROT_270) {
        return LCD_ST7789_ERR_PARAM;
    }

    apply_rotation(dev, rot);

    uint8_t madctl = madctl_for_rotation(rot);
    write_cmd_data(dev, LCD_ST7789_CMD_MADCTL, &madctl, 1);

    return LCD_ST7789_OK;
}

lcd_st7789_err_t lcd_st7789_invert(lcd_st7789_t *dev, bool on)
{
    if (!dev || !dev->spi) {
        return LCD_ST7789_ERR_PARAM;
    }
    write_cmd(dev, on ? LCD_ST7789_CMD_INVON : LCD_ST7789_CMD_INVOFF);
    return LCD_ST7789_OK;
}
