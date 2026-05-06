// SPDX-License-Identifier: MIT
/*
 * lcd_st7789 — ST7789 240x320 TFT LCD SPI driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * No heap, no OS. All hardware access through injected function pointers.
 * Supports 0/90/180/270 degree rotation and display inversion.
 */

#ifndef LCD_ST7789_H
#define LCD_ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LCD_ST7789_OK          =  0,
    LCD_ST7789_ERR_PARAM   = -1,
    LCD_ST7789_ERR_SPI     = -2,
} lcd_st7789_err_t;

/* ---------- rotation ---------- */

typedef enum {
    LCD_ST7789_ROT_0   = 0,
    LCD_ST7789_ROT_90  = 1,
    LCD_ST7789_ROT_180 = 2,
    LCD_ST7789_ROT_270 = 3,
} lcd_st7789_rotation_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);       /* single byte SPI xfer */
    void    (*dc_set)(void *ctx, bool level);      /* D/C pin: 0=cmd, 1=data */
    void    (*rst_set)(void *ctx, bool level);     /* RST pin: 0=reset, 1=normal */
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} lcd_st7789_spi_t;

/* ---------- configuration ---------- */

typedef struct {
    uint16_t width;        /* native panel width  (typ. 240) */
    uint16_t height;       /* native panel height (typ. 320) */
    uint16_t col_offset;   /* column start offset (some panels need this) */
    uint16_t row_offset;   /* row start offset    (some panels need this) */
} lcd_st7789_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const lcd_st7789_spi_t *spi;
    lcd_st7789_cfg_t        cfg;
    lcd_st7789_rotation_t   rotation;
    uint16_t                cur_width;   /* effective width  after rotation */
    uint16_t                cur_height;  /* effective height after rotation */
    uint16_t                col_offset;  /* effective offset after rotation */
    uint16_t                row_offset;  /* effective offset after rotation */
} lcd_st7789_t;

/* ---------- ST7789 command registers ---------- */

#define LCD_ST7789_CMD_NOP      0x00
#define LCD_ST7789_CMD_SWRESET  0x01
#define LCD_ST7789_CMD_SLPIN    0x10
#define LCD_ST7789_CMD_SLPOUT   0x11
#define LCD_ST7789_CMD_INVOFF   0x20
#define LCD_ST7789_CMD_INVON    0x21
#define LCD_ST7789_CMD_DISPOFF  0x28
#define LCD_ST7789_CMD_DISPON   0x29
#define LCD_ST7789_CMD_CASET    0x2A
#define LCD_ST7789_CMD_RASET    0x2B
#define LCD_ST7789_CMD_RAMWR    0x2C
#define LCD_ST7789_CMD_MADCTL   0x36
#define LCD_ST7789_CMD_COLMOD   0x3A

/* MADCTL bits */
#define LCD_ST7789_MADCTL_MY    0x80
#define LCD_ST7789_MADCTL_MX    0x40
#define LCD_ST7789_MADCTL_MV    0x20
#define LCD_ST7789_MADCTL_RGB   0x00
#define LCD_ST7789_MADCTL_BGR   0x08

/* COLMOD values */
#define LCD_ST7789_COLMOD_RGB565  0x55
#define LCD_ST7789_COLMOD_RGB666  0x66

/* ---------- API ---------- */

/**
 * Initialize display: hardware reset, sleep out, set color mode RGB565,
 * configure MADCTL for rotation 0, display on.
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_init(lcd_st7789_t *dev,
                                 const lcd_st7789_spi_t *spi,
                                 const lcd_st7789_cfg_t *cfg);

/**
 * Set drawing window (column/row address).
 * Coordinates are in current rotation frame.
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_set_window(lcd_st7789_t *dev,
                                       uint16_t x0, uint16_t y0,
                                       uint16_t x1, uint16_t y1);

/**
 * Write raw pixel data (RGB565, big-endian on wire) to current window.
 * Caller must set window first via lcd_st7789_set_window().
 *
 * @param data  Pointer to RGB565 pixel data (2 bytes per pixel, MSB first)
 * @param len   Number of bytes (must be even)
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_write_pixels(lcd_st7789_t *dev,
                                         const uint8_t *data,
                                         size_t len);

/**
 * Fill a rectangular region with a single RGB565 color.
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_fill_rect(lcd_st7789_t *dev,
                                      uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h,
                                      uint16_t color);

/** Turn display on (exit sleep is NOT implied). @thread_safe: no @isr_safe: no */
lcd_st7789_err_t lcd_st7789_display_on(lcd_st7789_t *dev);

/** Turn display off. @thread_safe: no @isr_safe: no */
lcd_st7789_err_t lcd_st7789_display_off(lcd_st7789_t *dev);

/**
 * Set display rotation (0/90/180/270 degrees).
 * Updates MADCTL register and swaps effective width/height + offsets.
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_set_rotation(lcd_st7789_t *dev,
                                         lcd_st7789_rotation_t rot);

/**
 * Enable or disable display color inversion.
 *
 * @thread_safe: no @isr_safe: no
 */
lcd_st7789_err_t lcd_st7789_invert(lcd_st7789_t *dev, bool on);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ST7789_H */
