// SPDX-License-Identifier: MIT
/*
 * tm1637 -- TM1637 4-digit 7-segment LED display driver (bit-bang)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "tm1637.h"
#include "contract.h"
#include <string.h>

/* ---------- segment encoding table ---------- */

/*
 * Standard 7-segment mapping:
 *   bit0=A(top), bit1=B(top-right), bit2=C(bot-right), bit3=D(bottom),
 *   bit4=E(bot-left), bit5=F(top-left), bit6=G(middle), bit7=colon/DP
 */
static const uint8_t digit_to_seg[16] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */
    0x66, /* 4 */  0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */
    0x7F, /* 8 */  0x6F, /* 9 */  0x77, /* A */  0x7C, /* b */
    0x39, /* C */  0x5E, /* d */  0x79, /* E */  0x71, /* F */
};

#define SEG_MINUS  0x40  /* segment G only (middle dash) */
#define SEG_BLANK  0x00

/* Bit timing delay in microseconds */
#define BIT_DELAY_US  2

/* ---------- low-level protocol helpers ---------- */

static inline void bit_delay(const tm1637_gpio_t *g)
{
    g->delay_us(g->ctx, BIT_DELAY_US);
}

/*
 * TM1637 start condition: DIO goes low while CLK is high.
 */
static void tm1637_start(const tm1637_gpio_t *g)
{
    g->dio_high(g->ctx);
    g->clk_high(g->ctx);
    bit_delay(g);
    g->dio_low(g->ctx);
    bit_delay(g);
    g->clk_low(g->ctx);
    bit_delay(g);
}

/*
 * TM1637 stop condition: DIO goes high while CLK is high.
 */
static void tm1637_stop(const tm1637_gpio_t *g)
{
    g->clk_low(g->ctx);
    bit_delay(g);
    g->dio_low(g->ctx);
    bit_delay(g);
    g->clk_high(g->ctx);
    bit_delay(g);
    g->dio_high(g->ctx);
    bit_delay(g);
}

/*
 * Write one byte LSB-first, return true if ACK received.
 */
static bool tm1637_write_byte(const tm1637_gpio_t *g, uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        g->clk_low(g->ctx);
        bit_delay(g);
        if (byte & (uint8_t)(1U << i)) {
            g->dio_high(g->ctx);
        } else {
            g->dio_low(g->ctx);
        }
        bit_delay(g);
        g->clk_high(g->ctx);
        bit_delay(g);
    }

    /* ACK bit: clock low, release DIO, clock high, read DIO */
    g->clk_low(g->ctx);
    g->dio_high(g->ctx);  /* release DIO for slave to pull low */
    bit_delay(g);
    g->clk_high(g->ctx);
    bit_delay(g);
    bool ack = !g->dio_read(g->ctx);  /* ACK = DIO pulled low */
    g->clk_low(g->ctx);
    bit_delay(g);

    return ack;
}

/*
 * Send a single-byte command (start + byte + stop).
 */
static bool tm1637_send_cmd(const tm1637_gpio_t *g, uint8_t cmd)
{
    tm1637_start(g);
    bool ok = tm1637_write_byte(g, cmd);
    tm1637_stop(g);
    return ok;
}

/*
 * Flush the shadow buffer to the display using auto-increment mode.
 */
static tm1637_err_t tm1637_flush(tm1637_t *dev)
{
    const tm1637_gpio_t *g = dev->gpio;

    /* Data command: auto-increment */
    if (!tm1637_send_cmd(g, TM1637_CMD_DATA_AUTO)) {
        return TM1637_ERR_ACK;
    }

    /* Address command + data */
    tm1637_start(g);
    if (!tm1637_write_byte(g, TM1637_CMD_ADDR_BASE)) {
        tm1637_stop(g);
        return TM1637_ERR_ACK;
    }
    for (uint8_t i = 0; i < TM1637_DIGITS; i++) {
        if (!tm1637_write_byte(g, dev->segments[i])) {
            tm1637_stop(g);
            return TM1637_ERR_ACK;
        }
    }
    tm1637_stop(g);

    /* Display control command */
    if (!tm1637_send_cmd(g, dev->brightness)) {
        return TM1637_ERR_ACK;
    }

    return TM1637_OK;
}

/*
 * Write a single segment at a fixed address.
 */
static tm1637_err_t tm1637_write_fixed(tm1637_t *dev, uint8_t pos,
                                       uint8_t seg)
{
    const tm1637_gpio_t *g = dev->gpio;

    /* Data command: fixed address */
    if (!tm1637_send_cmd(g, TM1637_CMD_DATA_FIXED)) {
        return TM1637_ERR_ACK;
    }

    /* Address + data */
    tm1637_start(g);
    if (!tm1637_write_byte(g, (uint8_t)(TM1637_CMD_ADDR_BASE + pos))) {
        tm1637_stop(g);
        return TM1637_ERR_ACK;
    }
    if (!tm1637_write_byte(g, seg)) {
        tm1637_stop(g);
        return TM1637_ERR_ACK;
    }
    tm1637_stop(g);

    /* Display control */
    if (!tm1637_send_cmd(g, dev->brightness)) {
        return TM1637_ERR_ACK;
    }

    return TM1637_OK;
}

/* ---------- public API ---------- */

tm1637_err_t tm1637_init(tm1637_t *dev, const tm1637_gpio_t *gpio)
{
    if (!dev || !gpio) {
        return TM1637_ERR_PARAM;
    }
    if (!gpio->clk_low || !gpio->clk_high || !gpio->dio_low ||
        !gpio->dio_high || !gpio->dio_read || !gpio->delay_us) {
        return TM1637_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->gpio = gpio;
    dev->brightness = TM1637_CMD_DISP_ON | 2;  /* on, brightness 2 */

    /* Clear display on init */
    return tm1637_flush(dev);
}

tm1637_err_t tm1637_set_brightness(tm1637_t *dev, uint8_t level, bool on)
{
    if (!dev || !dev->gpio) {
        return TM1637_ERR_PARAM;
    }
    if (level > 7) {
        return TM1637_ERR_PARAM;
    }

    if (on) {
        dev->brightness = (uint8_t)(TM1637_CMD_DISP_ON | level);
    } else {
        dev->brightness = TM1637_CMD_DISP_OFF;
    }

    /* Send display control command to apply immediately */
    if (!tm1637_send_cmd(dev->gpio, dev->brightness)) {
        return TM1637_ERR_ACK;
    }

    return TM1637_OK;
}

tm1637_err_t tm1637_display_segments(tm1637_t *dev,
                                     const uint8_t *segments, uint8_t count)
{
    if (!dev || !dev->gpio || !segments) {
        return TM1637_ERR_PARAM;
    }
    if (count == 0 || count > TM1637_DIGITS) {
        return TM1637_ERR_PARAM;
    }

    for (uint8_t i = 0; i < count; i++) {
        dev->segments[i] = segments[i];
    }

    return tm1637_flush(dev);
}

tm1637_err_t tm1637_display_number(tm1637_t *dev, int16_t number,
                                   bool leading_zeros)
{
    if (!dev || !dev->gpio) {
        return TM1637_ERR_PARAM;
    }

    bool negative = false;
    uint16_t abs_val;

    if (number < 0) {
        negative = true;
        /* Clamp to -999 (minus sign takes one digit) */
        if (number < -999) {
            number = -999;
        }
        abs_val = (uint16_t)(-number);
    } else {
        /* Clamp to 9999 */
        if (number > 9999) {
            number = 9999;
        }
        abs_val = (uint16_t)number;
    }

    /* Extract digits, rightmost first */
    uint8_t digits[TM1637_DIGITS];
    for (uint8_t i = 0; i < TM1637_DIGITS; i++) {
        digits[TM1637_DIGITS - 1 - i] = (uint8_t)(abs_val % 10);
        abs_val = abs_val / 10;
    }

    /* Determine how many digits the number actually needs */
    uint8_t first_nonzero = TM1637_DIGITS - 1; /* at least show ones place */
    for (uint8_t i = 0; i < TM1637_DIGITS - 1; i++) {
        if (digits[i] != 0) {
            first_nonzero = i;
            break;
        }
    }

    /* Build segment output */
    uint8_t segs[TM1637_DIGITS];
    for (uint8_t i = 0; i < TM1637_DIGITS; i++) {
        if (i < first_nonzero) {
            if (negative && i == first_nonzero - 1) {
                segs[i] = SEG_MINUS;
            } else if (leading_zeros) {
                segs[i] = digit_to_seg[0];
            } else if (negative && i >= first_nonzero - 1) {
                segs[i] = SEG_MINUS;
            } else {
                segs[i] = SEG_BLANK;
            }
        } else {
            segs[i] = digit_to_seg[digits[i]];
        }
    }

    /* If negative, place minus sign just before the first digit */
    if (negative) {
        if (first_nonzero > 0) {
            segs[first_nonzero - 1] = SEG_MINUS;
            /* blank everything before the minus */
            for (uint8_t i = 0; i < first_nonzero - 1; i++) {
                if (!leading_zeros) {
                    segs[i] = SEG_BLANK;
                }
            }
        }
        /* If first_nonzero == 0, all 4 digits used, no room for minus.
         * This can't happen since we clamped to -999 (3 digits + sign = 4). */
    }

    memcpy(dev->segments, segs, TM1637_DIGITS);
    return tm1637_flush(dev);
}

tm1637_err_t tm1637_display_digit(tm1637_t *dev, uint8_t position,
                                  uint8_t digit)
{
    if (!dev || !dev->gpio) {
        return TM1637_ERR_PARAM;
    }
    if (position >= TM1637_DIGITS || digit > 15) {
        return TM1637_ERR_PARAM;
    }

    uint8_t seg = digit_to_seg[digit];
    /* Preserve colon bit on position 1 */
    if (position == 1) {
        seg = (uint8_t)(seg | (dev->segments[1] & 0x80));
    }
    dev->segments[position] = seg;

    return tm1637_write_fixed(dev, position, dev->segments[position]);
}

tm1637_err_t tm1637_set_colon(tm1637_t *dev, bool on)
{
    if (!dev || !dev->gpio) {
        return TM1637_ERR_PARAM;
    }

    if (on) {
        dev->segments[1] |= 0x80;
    } else {
        dev->segments[1] &= 0x7F;
    }

    return tm1637_write_fixed(dev, 1, dev->segments[1]);
}

tm1637_err_t tm1637_clear(tm1637_t *dev)
{
    if (!dev || !dev->gpio) {
        return TM1637_ERR_PARAM;
    }

    memset(dev->segments, 0, sizeof(dev->segments));
    return tm1637_flush(dev);
}

uint8_t tm1637_encode_digit(uint8_t digit)
{
    if (digit > 15) {
        return 0x00;
    }
    return digit_to_seg[digit];
}
