// SPDX-License-Identifier: MIT
/*
 * gc9a01 — GC9A01 240x240 round TFT LCD SPI driver (RGB565)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * No heap, no OS. All hardware access through injected function pointers.
 * Supports 0/90/180/270 degree rotation, sleep, and display on/off control.
 */

#ifndef GC9A01_H
#define GC9A01_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    GC9A01_OK          =  0,
    GC9A01_ERR_PARAM   = -1,
    GC9A01_ERR_SPI     = -2,
} gc9a01_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);       /* single byte SPI xfer */
    void    (*dc_cmd)(void *ctx);                  /* DC pin low  (command) */
    void    (*dc_data)(void *ctx);                 /* DC pin high (data)    */
    void    (*rst_low)(void *ctx);                 /* RST pin low  (reset)  */
    void    (*rst_high)(void *ctx);                /* RST pin high (normal) */
    void    (*delay_ms)(void *ctx, uint16_t ms);
    void    *ctx;
} gc9a01_spi_t;

/* ---------- device context ---------- */

typedef struct {
    const gc9a01_spi_t *spi;
    uint8_t             rotation;     /* 0-3 */
    uint16_t            width;        /* effective width  after rotation */
    uint16_t            height;       /* effective height after rotation */
} gc9a01_t;

/* ---------- GC9A01 command registers ---------- */

#define GC9A01_CMD_NOP      0x00
#define GC9A01_CMD_SWRESET  0x01
#define GC9A01_CMD_SLPIN    0x10
#define GC9A01_CMD_SLPOUT   0x11
#define GC9A01_CMD_INVOFF   0x20
#define GC9A01_CMD_INVON    0x21
#define GC9A01_CMD_DISPOFF  0x28
#define GC9A01_CMD_DISPON   0x29
#define GC9A01_CMD_CASET    0x2A
#define GC9A01_CMD_RASET    0x2B
#define GC9A01_CMD_RAMWR    0x2C
#define GC9A01_CMD_MADCTL   0x36
#define GC9A01_CMD_COLMOD   0x3A

/* MADCTL bits */
#define GC9A01_MADCTL_MY    0x80
#define GC9A01_MADCTL_MX    0x40
#define GC9A01_MADCTL_MV    0x20
#define GC9A01_MADCTL_ML    0x10
#define GC9A01_MADCTL_BGR   0x08
#define GC9A01_MADCTL_MH    0x04

/* COLMOD values */
#define GC9A01_COLMOD_RGB565  0x05
#define GC9A01_COLMOD_RGB666  0x06

/* Display dimensions (fixed for GC9A01 round LCD) */
#define GC9A01_WIDTH   240
#define GC9A01_HEIGHT  240

/* ---------- API ---------- */

/**
 * Initialize display: hardware reset, sleep out, set color mode RGB565,
 * configure MADCTL for rotation 0, inversion on, display on.
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_init(gc9a01_t *dev, const gc9a01_spi_t *spi);

/**
 * Set drawing window (column/row address).
 * Coordinates are inclusive: (x0,y0) to (x1,y1).
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_set_window(gc9a01_t *dev,
                               uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1);

/**
 * Write raw pixel data (RGB565) to current window.
 * Caller must set window first via gc9a01_set_window().
 * Pixels are sent big-endian on wire (MSB first).
 *
 * @param pixels  Pointer to RGB565 pixel array
 * @param count   Number of pixels (not bytes)
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_write_pixels(gc9a01_t *dev,
                                 const uint16_t *pixels, uint32_t count);

/**
 * Fill the entire display with a single RGB565 color.
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_fill(gc9a01_t *dev, uint16_t color);

/**
 * Set display rotation (0-3, corresponding to 0/90/180/270 degrees).
 * Updates MADCTL register. For a 240x240 display the effective
 * width/height does not change, but pixel addressing order does.
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_set_rotation(gc9a01_t *dev, uint8_t rotation);

/**
 * Enter or exit sleep mode. Either way this waits the 120 ms the display
 * needs before the opposite command.
 *
 * @param enable  true = enter sleep, false = exit sleep
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_sleep(gc9a01_t *dev, bool enable);

/**
 * Turn display on or off.
 *
 * @param on  true = display on, false = display off
 *
 * @thread_safe: no @isr_safe: no
 */
gc9a01_err_t gc9a01_display(gc9a01_t *dev, bool on);

/**
 * Convert 8-bit R/G/B to RGB565 packed value.
 *
 * @return RGB565 16-bit color
 */
uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* GC9A01_H */
