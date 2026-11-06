// SPDX-License-Identifier: MIT
/*
 * ht16k33 — HT16K33 I2C LED matrix/segment controller driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * HT16K33 command set (datasheet):
 *   0x20 | bit0  System oscillator off/on (0x20=off, 0x21=on)
 *   0x80 | bit0  Display off/on (0x80=off, 0x81=on)
 *   0xE0 | duty  Dimming set (duty = 0..15, 1/16 to 16/16 duty cycle)
 *   0x00..0x0F   Display RAM addresses (16 bytes total)
 */

#include "ht16k33.h"
#include "contract.h"
#include <string.h>

/* ---------- command bytes ---------- */

#define CMD_OSCILLATOR_ON   0x21u
#define CMD_DISPLAY_ON      0x81u
#define CMD_BRIGHTNESS_BASE 0xE0u

/* ---------- internal helpers ---------- */

/*
 * Send a single-byte command to the HT16K33.
 *
 * HT16K33 commands are single-byte: [addr+W][cmd].
 * We abuse write_reg(addr, cmd, 0x00) — the chip ignores the extra
 * data byte for command registers. This avoids requiring a separate
 * write_cmd HAL function.
 */
static bool send_cmd(const ht16k33_t *dev, uint8_t cmd)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->addr, cmd, 0x00);
}

/* The steps of init that talk to the chip */
static ht16k33_err_t bring_up(const ht16k33_t *dev, uint8_t brightness)
{
    /* Turn on system oscillator */
    if (!send_cmd(dev, CMD_OSCILLATOR_ON)) {
        return HT16K33_ERR_I2C;
    }

    /* Turn on display */
    if (!send_cmd(dev, CMD_DISPLAY_ON)) {
        return HT16K33_ERR_I2C;
    }

    /* Set brightness */
    uint8_t bright_cmd = (uint8_t)(CMD_BRIGHTNESS_BASE | brightness);
    if (!send_cmd(dev, bright_cmd)) {
        return HT16K33_ERR_I2C;
    }

    return HT16K33_OK;
}

/* ---------- public API ---------- */

ht16k33_err_t ht16k33_init(ht16k33_t *dev,
                            const ht16k33_i2c_t *i2c,
                            const ht16k33_cfg_t *cfg)
{
    if (!dev) {
        return HT16K33_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg) {
        return HT16K33_ERR_PARAM;
    }
    if (!i2c->write_reg || !i2c->read_regs) {
        return HT16K33_ERR_PARAM;
    }
    /* Valid HT16K33 addresses: 0x70 through 0x77 (A0-A2 pins) */
    if (cfg->addr < 0x70u || cfg->addr > 0x77u) {
        return HT16K33_ERR_PARAM;
    }
    if (cfg->brightness > HT16K33_BRIGHTNESS_MAX) {
        return HT16K33_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->addr = cfg->addr;

    ht16k33_err_t err = bring_up(dev, cfg->brightness);
    if (err != HT16K33_OK) {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return HT16K33_OK;
}

ht16k33_err_t ht16k33_set_brightness(ht16k33_t *dev, uint8_t level)
{
    if (!dev || !dev->i2c) {
        return HT16K33_ERR_PARAM;
    }
    if (level > HT16K33_BRIGHTNESS_MAX) {
        return HT16K33_ERR_PARAM;
    }

    uint8_t bright_cmd = (uint8_t)(CMD_BRIGHTNESS_BASE | level);
    if (!send_cmd(dev, bright_cmd)) {
        return HT16K33_ERR_I2C;
    }

    return HT16K33_OK;
}

ht16k33_err_t ht16k33_write_raw(ht16k33_t *dev,
                                const uint8_t *buf, uint8_t len)
{
    if (!dev || !dev->i2c || !buf) {
        return HT16K33_ERR_PARAM;
    }
    if (len == 0 || len > HT16K33_DISPLAY_RAM_SIZE) {
        return HT16K33_ERR_PARAM;
    }

    /* Write each byte to display RAM starting at address 0x00 */
    for (uint8_t i = 0; i < len; i++) {
        if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, i, buf[i])) {
            return HT16K33_ERR_I2C;
        }
    }

    return HT16K33_OK;
}

ht16k33_err_t ht16k33_clear(ht16k33_t *dev)
{
    if (!dev || !dev->i2c) {
        return HT16K33_ERR_PARAM;
    }

    /* Zero all 16 display RAM bytes */
    for (uint8_t i = 0; i < HT16K33_DISPLAY_RAM_SIZE; i++) {
        if (!dev->i2c->write_reg(dev->i2c->ctx, dev->addr, i, 0x00)) {
            return HT16K33_ERR_I2C;
        }
    }

    return HT16K33_OK;
}
