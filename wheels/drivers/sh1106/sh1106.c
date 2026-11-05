// SPDX-License-Identifier: MIT
/*
 * sh1106 — SH1106 132x64 OLED display controller driver (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * SH1106 uses page addressing (not horizontal addressing like SSD1306).
 * Each page flush requires setting the page address and column address
 * (with 2-column offset). I2C control byte: 0x00 = command, 0x40 = data.
 */

#include "sh1106.h"
#include "contract.h"
#include <string.h>

/* ---- SH1106 commands ---- */
#define CMD_DISPLAY_OFF         0xAEu
#define CMD_DISPLAY_ON          0xAFu
#define CMD_SET_MUX_RATIO       0xA8u
#define CMD_SET_DISPLAY_OFFSET  0xD3u
#define CMD_SET_START_LINE      0x40u
#define CMD_SET_SEG_REMAP       0xA1u   /* column address 131 mapped to SEG0 */
#define CMD_SET_COM_SCAN_DEC    0xC8u   /* scan from COM63 to COM0 */
#define CMD_SET_COM_PINS        0xDAu
#define CMD_SET_CONTRAST        0x81u
#define CMD_SET_ENTIRE_ON_RESUME 0xA4u
#define CMD_SET_NORMAL_DISPLAY  0xA6u
#define CMD_SET_CLK_DIV         0xD5u
#define CMD_SET_PRECHARGE       0xD9u
#define CMD_SET_VCOMH           0xDBu
#define CMD_SET_PAGE_ADDR       0xB0u   /* | page (0-7) */
#define CMD_SET_COL_LOW         0x00u   /* | low nibble */
#define CMD_SET_COL_HIGH        0x10u   /* | high nibble */

/* I2C control bytes */
#define CTRL_CMD                0x00u
#define CTRL_DATA               0x40u

static bool cmd(const sh1106_t *dev, uint8_t c)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, CTRL_CMD, c);
}

static bool write_data_page(const sh1106_t *dev, uint8_t page,
                            const uint8_t *data)
{
    /* Set page address */
    if (!cmd(dev, (uint8_t)(CMD_SET_PAGE_ADDR | (page & 0x07u))))
        return false;

    /* Set column address with 2-column offset */
    uint8_t col = (uint8_t)SH1106_COL_OFFSET;
    if (!cmd(dev, (uint8_t)(CMD_SET_COL_LOW | (col & 0x0Fu))))
        return false;
    if (!cmd(dev, (uint8_t)(CMD_SET_COL_HIGH | ((col >> 4) & 0x0Fu))))
        return false;

    /* Write 128 bytes of page data via data control byte */
    for (uint8_t i = 0; i < SH1106_WIDTH; i++)
    {
        if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, CTRL_DATA, data[i]))
            return false;
    }
    return true;
}

sh1106_err_t sh1106_init(sh1106_t *dev, const sh1106_i2c_t *i2c,
                         uint8_t addr, uint8_t *framebuf)
{
    if (!dev || !i2c || !framebuf)
        return SH1106_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return SH1106_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c  = i2c;
    dev->addr = addr;
    dev->buf  = framebuf;
    memset(dev->buf, 0, SH1106_BUF_SIZE);

    /* Initialization sequence */
    if (!cmd(dev, CMD_DISPLAY_OFF))          return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_CLK_DIV))          return SH1106_ERR_I2C;
    if (!cmd(dev, 0x80))                     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_MUX_RATIO))        return SH1106_ERR_I2C;
    if (!cmd(dev, 0x3F))                     return SH1106_ERR_I2C;  /* 64 - 1 */
    if (!cmd(dev, CMD_SET_DISPLAY_OFFSET))   return SH1106_ERR_I2C;
    if (!cmd(dev, 0x00))                     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_START_LINE))       return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_SEG_REMAP))        return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_COM_SCAN_DEC))     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_COM_PINS))         return SH1106_ERR_I2C;
    if (!cmd(dev, 0x12))                     return SH1106_ERR_I2C;  /* alt COM pin */
    if (!cmd(dev, CMD_SET_CONTRAST))         return SH1106_ERR_I2C;
    if (!cmd(dev, 0x80))                     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_PRECHARGE))        return SH1106_ERR_I2C;
    if (!cmd(dev, 0xF1))                     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_VCOMH))            return SH1106_ERR_I2C;
    if (!cmd(dev, 0x40))                     return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_ENTIRE_ON_RESUME)) return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_SET_NORMAL_DISPLAY))   return SH1106_ERR_I2C;

    /* Reset leaves display RAM as it was, so clear it before switching on. */
    if (sh1106_display(dev) != SH1106_OK)    return SH1106_ERR_I2C;
    if (!cmd(dev, CMD_DISPLAY_ON))           return SH1106_ERR_I2C;

    INVARIANT(dev->i2c != NULL);
    INVARIANT(dev->buf != NULL);
    return SH1106_OK;
}

sh1106_err_t sh1106_display(sh1106_t *dev)
{
    if (!dev || !dev->buf)
        return SH1106_ERR_PARAM;

    for (uint8_t page = 0; page < SH1106_PAGES; page++)
    {
        if (!write_data_page(dev, page,
                             &dev->buf[(size_t)page * SH1106_WIDTH]))
        {
            return SH1106_ERR_I2C;
        }
    }
    return SH1106_OK;
}

void sh1106_clear(sh1106_t *dev)
{
    if (!dev || !dev->buf)
        return;
    memset(dev->buf, 0, SH1106_BUF_SIZE);
}

void sh1106_set_pixel(sh1106_t *dev, uint8_t x, uint8_t y, bool on)
{
    if (!dev || !dev->buf || x >= SH1106_WIDTH || y >= SH1106_HEIGHT)
        return;

    size_t idx = (size_t)x + ((size_t)(y / 8u) * SH1106_WIDTH);
    uint8_t bit = (uint8_t)(1u << (y % 8u));
    if (on)
        dev->buf[idx] |= bit;
    else
        dev->buf[idx] &= (uint8_t)~bit;
}

bool sh1106_get_pixel(const sh1106_t *dev, uint8_t x, uint8_t y)
{
    if (!dev || !dev->buf || x >= SH1106_WIDTH || y >= SH1106_HEIGHT)
        return false;

    size_t idx = (size_t)x + ((size_t)(y / 8u) * SH1106_WIDTH);
    return (dev->buf[idx] & (1u << (y % 8u))) != 0;
}

void sh1106_fill(sh1106_t *dev, bool on)
{
    if (!dev || !dev->buf)
        return;
    memset(dev->buf, on ? 0xFF : 0x00, SH1106_BUF_SIZE);
}

sh1106_err_t sh1106_set_contrast(sh1106_t *dev, uint8_t val)
{
    if (!dev)
        return SH1106_ERR_PARAM;
    if (!cmd(dev, CMD_SET_CONTRAST)) return SH1106_ERR_I2C;
    if (!cmd(dev, val))              return SH1106_ERR_I2C;
    return SH1106_OK;
}

sh1106_err_t sh1106_display_on(const sh1106_t *dev)
{
    if (!dev) return SH1106_ERR_PARAM;
    if (!cmd(dev, CMD_DISPLAY_ON)) return SH1106_ERR_I2C;
    return SH1106_OK;
}

sh1106_err_t sh1106_display_off(const sh1106_t *dev)
{
    if (!dev) return SH1106_ERR_PARAM;
    if (!cmd(dev, CMD_DISPLAY_OFF)) return SH1106_ERR_I2C;
    return SH1106_OK;
}
