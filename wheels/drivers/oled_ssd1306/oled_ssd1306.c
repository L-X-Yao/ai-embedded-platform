// SPDX-License-Identifier: MIT
/*
 * oled_ssd1306 — SSD1306 128x64 OLED display driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "oled_ssd1306.h"
#include <string.h>

static bool cmd(const oled_ssd1306_t *dev, uint8_t c)
{
    return dev->i2c.write_cmd(dev->i2c.ctx, dev->cfg.addr, c);
}

oled_ssd1306_err_t oled_ssd1306_init(oled_ssd1306_t *dev,
                                     const oled_ssd1306_i2c_t *i2c,
                                     const oled_ssd1306_cfg_t *cfg,
                                     uint8_t *framebuf)
{
    if (!dev || !i2c || !cfg || !framebuf)
    {
        return OLED_SSD1306_ERR_PARAM;
    }
    if (cfg->width == 0 || cfg->height == 0 || (cfg->height % 8) != 0)
    {
        return OLED_SSD1306_ERR_PARAM;
    }

    if (!i2c->write_cmd || !i2c->write_data) {
        return OLED_SSD1306_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;
    dev->buf = framebuf;
    dev->buf_size = (size_t)cfg->width * (cfg->height / 8);
    memset(dev->buf, 0, dev->buf_size);

    if (!cmd(dev, 0xAE)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xD5)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x80)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xA8)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, (uint8_t)(cfg->height - 1))) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xD3)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x00)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x40)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x8D)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x14)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x20)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x00)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xA1)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xC8)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xDA)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, cfg->height == 32 ? 0x02 : 0x12)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x81)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xCF)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xD9)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xF1)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xDB)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x40)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xA4)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xA6)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0xAF)) return OLED_SSD1306_ERR_I2C;

    return OLED_SSD1306_OK;
}

oled_ssd1306_err_t oled_ssd1306_flush(oled_ssd1306_t *dev)
{
    if (!dev)
    {
        return OLED_SSD1306_ERR_PARAM;
    }

    if (!cmd(dev, 0x21)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x00)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, (uint8_t)(dev->cfg.width - 1))) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x22)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, 0x00)) return OLED_SSD1306_ERR_I2C;
    if (!cmd(dev, (uint8_t)(dev->cfg.height / 8 - 1))) return OLED_SSD1306_ERR_I2C;

    if (!dev->i2c.write_data(dev->i2c.ctx, dev->cfg.addr,
                             dev->buf, (uint16_t)dev->buf_size))
    {
        return OLED_SSD1306_ERR_I2C;
    }

    return OLED_SSD1306_OK;
}

void oled_ssd1306_clear(oled_ssd1306_t *dev)
{
    if (!dev || !dev->buf)
    {
        return;
    }
    memset(dev->buf, 0, dev->buf_size);
}

void oled_ssd1306_set_pixel(oled_ssd1306_t *dev, uint8_t x, uint8_t y, bool on)
{
    if (!dev || !dev->buf || x >= dev->cfg.width || y >= dev->cfg.height)
    {
        return;
    }
    size_t idx = (size_t)x + ((size_t)(y / 8) * dev->cfg.width);
    uint8_t bit = (uint8_t)(1 << (y % 8));
    if (on)
    {
        dev->buf[idx] |= bit;
    }
    else
    {
        dev->buf[idx] &= (uint8_t)~bit;
    }
}

bool oled_ssd1306_get_pixel(const oled_ssd1306_t *dev, uint8_t x, uint8_t y)
{
    if (!dev || !dev->buf || x >= dev->cfg.width || y >= dev->cfg.height)
    {
        return false;
    }
    size_t idx = (size_t)x + ((size_t)(y / 8) * dev->cfg.width);
    return (dev->buf[idx] & (1 << (y % 8))) != 0;
}

void oled_ssd1306_fill(oled_ssd1306_t *dev, bool on)
{
    if (!dev || !dev->buf)
    {
        return;
    }
    memset(dev->buf, on ? 0xFF : 0x00, dev->buf_size);
}

oled_ssd1306_err_t oled_ssd1306_display_on(const oled_ssd1306_t *dev)
{
    if (!dev) return OLED_SSD1306_ERR_PARAM;
    if (!cmd(dev, 0xAF)) return OLED_SSD1306_ERR_I2C;
    return OLED_SSD1306_OK;
}

oled_ssd1306_err_t oled_ssd1306_display_off(const oled_ssd1306_t *dev)
{
    if (!dev) return OLED_SSD1306_ERR_PARAM;
    if (!cmd(dev, 0xAE)) return OLED_SSD1306_ERR_I2C;
    return OLED_SSD1306_OK;
}
