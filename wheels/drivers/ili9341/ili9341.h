// SPDX-License-Identifier: MIT
/*
 * ili9341 — ILI9341 SPI TFT display driver (240x320, RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI + DC (data/command) + RST pin interface.
 * 16-bit RGB565 color mode. Supports 0/90/180/270 rotation.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ILI9341_OK          =  0,
    ILI9341_ERR_PARAM   = -1,
    ILI9341_ERR_SPI     = -2,
} ili9341_err_t;

/* ---------- rotation ---------- */

typedef enum {
    ILI9341_ROT_0   = 0,
    ILI9341_ROT_90  = 1,
    ILI9341_ROT_180 = 2,
    ILI9341_ROT_270 = 3,
} ili9341_rotation_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);       /* single byte SPI xfer */
    void    (*dc_set)(void *ctx, bool level);      /* D/C pin: 0=cmd, 1=data */
    void    (*rst_set)(void *ctx, bool level);     /* RST pin: 0=reset, 1=normal */
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} ili9341_spi_t;

/* ---------- ILI9341 command registers ---------- */

#define ILI9341_CMD_NOP      0x00u
#define ILI9341_CMD_SWRESET  0x01u
#define ILI9341_CMD_RDDID    0x04u
#define ILI9341_CMD_SLPIN    0x10u
#define ILI9341_CMD_SLPOUT   0x11u
#define ILI9341_CMD_INVOFF   0x20u
#define ILI9341_CMD_INVON    0x21u
#define ILI9341_CMD_DISPOFF  0x28u
#define ILI9341_CMD_DISPON   0x29u
#define ILI9341_CMD_CASET    0x2Au
#define ILI9341_CMD_RASET    0x2Bu
#define ILI9341_CMD_RAMWR    0x2Cu
#define ILI9341_CMD_MADCTL   0x36u
#define ILI9341_CMD_COLMOD   0x3Au

/* MADCTL bits */
#define ILI9341_MADCTL_MY    0x80u
#define ILI9341_MADCTL_MX    0x40u
#define ILI9341_MADCTL_MV    0x20u
#define ILI9341_MADCTL_ML    0x10u
#define ILI9341_MADCTL_BGR   0x08u
#define ILI9341_MADCTL_MH    0x04u

/* COLMOD values */
#define ILI9341_COLMOD_RGB565  0x55u
#define ILI9341_COLMOD_RGB666  0x66u

/* Display native dimensions */
#define ILI9341_WIDTH   240u
#define ILI9341_HEIGHT  320u

/* ---------- device context ---------- */

typedef struct {
    const ili9341_spi_t *spi;
    ili9341_rotation_t   rotation;
    uint16_t             cur_width;
    uint16_t             cur_height;
} ili9341_t;

/* ---------- API ---------- */

/**
 * Initialize display: hardware reset, sleep out, set RGB565,
 * configure MADCTL for rotation 0, display on.
 *
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_init(ili9341_t *dev,
                               const ili9341_spi_t *spi);

/**
 * Set display rotation (0/90/180/270 degrees).
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_set_rotation(ili9341_t *dev,
                                       ili9341_rotation_t rot);

/**
 * Set drawing window (column/row address).
 * Coordinates are in current rotation frame.
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_set_window(ili9341_t *dev,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1);

/**
 * Fill a rectangular region with a single RGB565 color.
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_fill_rect(ili9341_t *dev,
                                    uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h,
                                    uint16_t color);

/**
 * Draw a single pixel.
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_draw_pixel(ili9341_t *dev,
                                     uint16_t x, uint16_t y,
                                     uint16_t color);

/**
 * Fill entire screen with a single color.
 * @thread_safe: no @isr_safe: no
 */
ili9341_err_t ili9341_fill_screen(ili9341_t *dev, uint16_t color);

/**
 * Convert 8-bit R, G, B to RGB565 color value.
 * @thread_safe: yes @isr_safe: yes (pure function)
 */
uint16_t ili9341_rgb565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
