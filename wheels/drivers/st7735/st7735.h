// SPDX-License-Identifier: MIT
/*
 * st7735 — ST7735 128x160 SPI TFT LCD driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * No heap, no OS. All hardware access through injected function pointers.
 * Supports 128x160 pixel display with 16-bit RGB565 color format.
 */

#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ST7735_OK          =  0,
    ST7735_ERR_PARAM   = -1,
    ST7735_ERR_SPI     = -2,
} st7735_err_t;

/* ---------- display dimensions ---------- */

#define ST7735_WIDTH   128u
#define ST7735_HEIGHT  160u

/* ---------- ST7735 command registers ---------- */

#define ST7735_CMD_SLPOUT   0x11u
#define ST7735_CMD_NORON    0x13u
#define ST7735_CMD_DISPON   0x29u
#define ST7735_CMD_CASET    0x2Au
#define ST7735_CMD_RASET    0x2Bu
#define ST7735_CMD_RAMWR    0x2Cu
#define ST7735_CMD_MADCTL   0x36u
#define ST7735_CMD_COLMOD   0x3Au

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void *ctx;
    bool (*cs_select)(void *ctx);
    void (*cs_deselect)(void *ctx);
    bool (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*dc_command)(void *ctx);    /* DC pin LOW = command */
    void (*dc_data)(void *ctx);       /* DC pin HIGH = data */
    void (*reset)(void *ctx);         /* RST pin toggle */
    void (*delay_ms)(void *ctx, uint32_t ms);
} st7735_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const st7735_spi_t *spi;
} st7735_t;

/* ---------- API ---------- */

/** Initialize display: HW reset, sleep out, set RGB565, MADCTL, display on.
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_init(st7735_t *dev, const st7735_spi_t *spi);

/** Set drawing window (column/row address).
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_set_window(st7735_t *dev, uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1);

/** Write pixel data (RGB565 big-endian) to current window.
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_write_pixels(st7735_t *dev, const uint16_t *pixels,
                                  uint32_t count);

/** Fill a rectangular region with a single RGB565 color.
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_fill_rect(st7735_t *dev, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, uint16_t color);

/** Fill entire screen with a single RGB565 color.
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_fill_screen(st7735_t *dev, uint16_t color);

/** Draw a single pixel at (x, y) with RGB565 color.
 *  @thread_safe: no @isr_safe: no */
st7735_err_t st7735_draw_pixel(st7735_t *dev, uint16_t x, uint16_t y,
                                uint16_t color);

/** Convert 8-bit R, G, B to 16-bit RGB565. */
static inline uint16_t st7735_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                      ((uint16_t)(g & 0xFCu) << 3) |
                      ((uint16_t)b >> 3));
}

#ifdef __cplusplus
}
#endif

#endif /* ST7735_H */
