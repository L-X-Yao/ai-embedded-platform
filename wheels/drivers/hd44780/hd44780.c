// SPDX-License-Identifier: MIT
/*
 * hd44780 — HD44780 character LCD driver via PCF8574 I2C backpack
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 4-bit mode via PCF8574 I2C expander. Data is sent as two nibbles
 * with EN pulse for each nibble. PCF8574 pin mapping:
 *   bit 7:4 = D7:D4, bit 3 = BL, bit 2 = EN, bit 1 = RW, bit 0 = RS
 */

#include "hd44780.h"
#include "contract.h"
#include <string.h>

/* ---- PCF8574 pin mapping ---- */
#define PIN_RS   0x01u   /* Register select: 0=command, 1=data */
#define PIN_RW   0x02u   /* Read/Write: 0=write */
#define PIN_EN   0x04u   /* Enable pulse */
#define PIN_BL   0x08u   /* Backlight */

/* ---- HD44780 commands ---- */
#define LCD_CLEAR_DISPLAY    0x01u
#define LCD_RETURN_HOME      0x02u
#define LCD_ENTRY_MODE_SET   0x04u
#define LCD_DISPLAY_CONTROL  0x08u
#define LCD_FUNCTION_SET     0x20u
#define LCD_SET_DDRAM_ADDR   0x80u

/* Entry mode flags */
#define LCD_ENTRY_INC        0x02u   /* increment cursor */

/* Display control flags */
#define LCD_DISPLAY_ON       0x04u

/* Function set flags */
#define LCD_4BIT_MODE        0x00u
#define LCD_2LINE            0x08u
#define LCD_5X8_DOTS         0x00u

/* Row DDRAM start addresses */
static const uint8_t row_offsets[4] = { 0x00, 0x40, 0x14, 0x54 };

/* Write a raw byte to the PCF8574 expander */
static bool expander_write(const hd44780_t *dev, uint8_t data)
{
    /* PCF8574: writing to device address sets all 8 output pins.
     * We use write_reg with reg=data, val=data (the register byte IS the data
     * on PCF8574). Some I2C HALs send [addr, reg, val] which means the
     * expander sees two bytes -- only the last one sticks. To be compatible
     * with the standard write_reg HAL, we send data as the register byte. */
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, data, data);
}

static void delay(const hd44780_t *dev, uint32_t ms)
{
    dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

/* Pulse EN high then low */
static bool pulse_enable(const hd44780_t *dev, uint8_t data)
{
    if (!expander_write(dev, (uint8_t)(data | PIN_EN)))
        return false;
    /* EN pulse width min 450ns -- delay_ms(1) is sufficient */
    delay(dev, 1);
    if (!expander_write(dev, (uint8_t)(data & (uint8_t)~PIN_EN)))
        return false;
    return true;
}

/* Send 4 bits (high nibble of val) with given mode flags */
static bool write_nibble(const hd44780_t *dev, uint8_t nibble, uint8_t mode)
{
    uint8_t data = (uint8_t)((nibble & 0xF0u) | mode | dev->backlight);
    if (!expander_write(dev, data))
        return false;
    return pulse_enable(dev, data);
}

/* Send a full byte in 4-bit mode (high nibble first, then low nibble) */
static bool write_byte(const hd44780_t *dev, uint8_t val, uint8_t mode)
{
    if (!write_nibble(dev, val, mode))
        return false;
    if (!write_nibble(dev, (uint8_t)(val << 4), mode))
        return false;
    return true;
}

static hd44780_err_t send_cmd(const hd44780_t *dev, uint8_t cmd)
{
    if (!write_byte(dev, cmd, 0))
        return HD44780_ERR_I2C;
    return HD44780_OK;
}

static hd44780_err_t send_data(const hd44780_t *dev, uint8_t data)
{
    if (!write_byte(dev, data, PIN_RS))
        return HD44780_ERR_I2C;
    return HD44780_OK;
}

/* The steps of init that talk to the display */
static hd44780_err_t bring_up(const hd44780_t *dev)
{
    /* Wait >40ms after power-on */
    delay(dev, 50);

    /* Expander initial state: all low + backlight */
    if (!expander_write(dev, dev->backlight))
        return HD44780_ERR_I2C;
    delay(dev, 1);

    /* HD44780 reset sequence: send 0x30 three times in 8-bit mode,
     * then switch to 4-bit mode */
    if (!write_nibble(dev, 0x30, 0)) return HD44780_ERR_I2C;
    delay(dev, 5);   /* wait >4.1ms */
    if (!write_nibble(dev, 0x30, 0)) return HD44780_ERR_I2C;
    delay(dev, 1);   /* wait >100us */
    if (!write_nibble(dev, 0x30, 0)) return HD44780_ERR_I2C;
    delay(dev, 1);

    /* Set 4-bit interface */
    if (!write_nibble(dev, 0x20, 0)) return HD44780_ERR_I2C;
    delay(dev, 1);

    /* Function set: 4-bit, 2-line (if rows>1), 5x8 dots */
    uint8_t func = LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_5X8_DOTS;
    if (dev->rows > 1)
        func |= LCD_2LINE;
    if (send_cmd(dev, func) != HD44780_OK) return HD44780_ERR_I2C;
    delay(dev, 1);

    /* Display off while it is set up, as in Figure 24 */
    if (send_cmd(dev, LCD_DISPLAY_CONTROL) != HD44780_OK)
        return HD44780_ERR_I2C;
    delay(dev, 1);

    /* Clear display */
    if (send_cmd(dev, LCD_CLEAR_DISPLAY) != HD44780_OK)
        return HD44780_ERR_I2C;
    delay(dev, 2);

    /* Entry mode: increment, no shift */
    if (send_cmd(dev, (uint8_t)(LCD_ENTRY_MODE_SET | LCD_ENTRY_INC)) != HD44780_OK)
        return HD44780_ERR_I2C;
    delay(dev, 1);

    /* Display on, cursor off, blink off */
    if (send_cmd(dev, (uint8_t)(LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON)) != HD44780_OK)
        return HD44780_ERR_I2C;
    delay(dev, 1);

    return HD44780_OK;
}

hd44780_err_t hd44780_init(hd44780_t *dev, const hd44780_i2c_t *i2c,
                           uint8_t addr, uint8_t cols, uint8_t rows)
{
    if (!dev)
        return HD44780_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c)
        return HD44780_ERR_PARAM;
    if (!i2c->write_reg || !i2c->delay_ms)
        return HD44780_ERR_PARAM;
    if (cols == 0 || cols > HD44780_MAX_COLS || rows == 0 || rows > HD44780_MAX_ROWS)
        return HD44780_ERR_PARAM;
    /* Rows 2 and 3 are the tail of the two DDRAM lines (0x00-0x27, 0x40-0x67). */
    if (rows > 2 && cols > 20)
        return HD44780_ERR_PARAM;

    dev->i2c       = i2c;
    dev->addr      = addr;
    dev->cols      = cols;
    dev->rows      = rows;
    dev->backlight = PIN_BL;

    hd44780_err_t err = bring_up(dev);
    if (err != HD44780_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return HD44780_OK;
}

hd44780_err_t hd44780_clear(hd44780_t *dev)
{
    if (!dev || !dev->i2c)
        return HD44780_ERR_PARAM;
    hd44780_err_t err = send_cmd(dev, LCD_CLEAR_DISPLAY);
    if (err != HD44780_OK)
        return err;
    delay(dev, 2);  /* clear needs ~1.52ms */
    return HD44780_OK;
}

hd44780_err_t hd44780_set_cursor(hd44780_t *dev, uint8_t col, uint8_t row)
{
    if (!dev || !dev->i2c)
        return HD44780_ERR_PARAM;
    if (row >= dev->rows)
        return HD44780_ERR_PARAM;
    if (col >= dev->cols)
        return HD44780_ERR_PARAM;

    uint8_t addr = (uint8_t)(row_offsets[row] + col);
    return send_cmd(dev, (uint8_t)(LCD_SET_DDRAM_ADDR | addr));
}

hd44780_err_t hd44780_write_char(hd44780_t *dev, char ch)
{
    if (!dev || !dev->i2c)
        return HD44780_ERR_PARAM;
    return send_data(dev, (uint8_t)ch);
}

hd44780_err_t hd44780_write_str(hd44780_t *dev, const char *str)
{
    if (!dev || !dev->i2c || !str)
        return HD44780_ERR_PARAM;
    while (*str)
    {
        hd44780_err_t err = send_data(dev, (uint8_t)*str);
        if (err != HD44780_OK)
            return err;
        str++;
    }
    return HD44780_OK;
}

hd44780_err_t hd44780_backlight(hd44780_t *dev, bool on)
{
    if (!dev || !dev->i2c)
        return HD44780_ERR_PARAM;
    dev->backlight = on ? PIN_BL : 0;
    if (!expander_write(dev, dev->backlight))
        return HD44780_ERR_I2C;
    return HD44780_OK;
}
