// SPDX-License-Identifier: MIT
/*
 * hd44780 — HD44780 character LCD driver via PCF8574 I2C backpack
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Drives HD44780-compatible LCDs (16x2, 20x4, etc.) through a PCF8574
 * I2C GPIO expander in 4-bit mode. Pin mapping:
 *   D4-D7 = bits 4-7, EN = bit 2, RW = bit 1, RS = bit 0, BL = bit 3
 * Pure C99, no heap. delay_ms callback is required for LCD timing.
 */

#ifndef HD44780_H
#define HD44780_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HD44780_OK        =  0,
    HD44780_ERR_I2C   = -1,
    HD44780_ERR_ID    = -2,
    HD44780_ERR_PARAM = -3,
} hd44780_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} hd44780_i2c_t;

/* ---------- constants ---------- */

#define HD44780_ADDR_PCF8574   0x27   /* default PCF8574 address */
#define HD44780_ADDR_PCF8574A  0x3F   /* default PCF8574A address */
#define HD44780_MAX_COLS       40u
#define HD44780_MAX_ROWS       4u

/* ---------- device context ---------- */

typedef struct {
    const hd44780_i2c_t *i2c;
    uint8_t              addr;
    uint8_t              cols;
    uint8_t              rows;
    uint8_t              backlight;  /* BL bit state (0x08 or 0x00) */
} hd44780_t;

/* ---------- API ---------- */

/** Initialize LCD in 4-bit mode and configure display.
 *  @param dev   Device context (caller-owned)
 *  @param i2c   I2C HAL callbacks (delay_ms is required)
 *  @param addr  I2C address (0x27 or 0x3F typically)
 *  @param cols  Number of columns (e.g. 16 or 20)
 *  @param rows  Number of rows (1-4)
 *  @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_init(hd44780_t *dev, const hd44780_i2c_t *i2c,
                           uint8_t addr, uint8_t cols, uint8_t rows);

/** Clear display and return cursor to home. @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_clear(hd44780_t *dev);

/** Move cursor to (col, row). @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_set_cursor(hd44780_t *dev, uint8_t col, uint8_t row);

/** Write a single character at current cursor position. @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_write_char(hd44780_t *dev, char ch);

/** Write a null-terminated string. @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_write_str(hd44780_t *dev, const char *str);

/** Enable or disable backlight. @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_backlight(hd44780_t *dev, bool on);

#ifdef __cplusplus
}
#endif

#endif /* HD44780_H */
