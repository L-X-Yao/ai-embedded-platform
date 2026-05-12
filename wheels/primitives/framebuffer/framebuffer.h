// SPDX-License-Identifier: MIT
/*
 * framebuffer -- pixel buffer with drawing primitives for mono/RGB565 displays
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Caller provides the pixel buffer. Supports MONO_VLSB (SSD1306),
 * MONO_HLSB, and RGB565 formats. All drawing clips to bounds.
 * No heap, no OS.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    FRAMEBUFFER_OK        =  0,
    FRAMEBUFFER_ERR_PARAM = -1,
} framebuffer_err_t;

/* ---------- pixel format ---------- */

typedef enum {
    FB_FORMAT_MONO_VLSB = 0,  /* 1bpp, vertical LSB first (SSD1306) */
    FB_FORMAT_MONO_HLSB = 1,  /* 1bpp, horizontal LSB first */
    FB_FORMAT_RGB565    = 2,  /* 16bpp, RGB565 big-endian */
} fb_format_t;

/* ---------- instance ---------- */

typedef struct {
    uint8_t    *buf;     /* caller-provided pixel buffer */
    uint16_t    width;   /* display width in pixels */
    uint16_t    height;  /* display height in pixels */
    fb_format_t format;  /* pixel format */
    uint16_t    stride;  /* internal: bytes per row (computed at init) */
} framebuffer_t;

/* ---------- API ---------- */

/** Compute required buffer size in bytes for given dimensions and format.
 *  Returns 0 on invalid input.
 *  @thread_safe: yes @isr_safe: yes */
uint32_t framebuffer_buf_size(uint16_t width, uint16_t height,
                              fb_format_t format);

/** Initialize framebuffer with caller-provided buffer. Clears to black.
 *  @thread_safe: no @isr_safe: no */
framebuffer_err_t framebuffer_init(framebuffer_t *fb, uint8_t *buf,
                                   uint16_t width, uint16_t height,
                                   fb_format_t format);

/** Clear entire framebuffer to a single color.
 *  For mono formats: 0 = off, nonzero = on (fills 0x00 or 0xFF).
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_clear(framebuffer_t *fb, uint16_t color);

/** Set a single pixel. Out-of-bounds coordinates are silently clipped.
 *  For mono: 0 = off, nonzero = on.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_set_pixel(framebuffer_t *fb, uint16_t x, uint16_t y,
                           uint16_t color);

/** Get pixel value. Returns 0 for out-of-bounds or NULL.
 *  For mono: returns 0 or 1.
 *  @thread_safe: no @isr_safe: yes */
uint16_t framebuffer_get_pixel(const framebuffer_t *fb, uint16_t x,
                               uint16_t y);

/** Draw horizontal line. Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_hline(framebuffer_t *fb, uint16_t x, uint16_t y,
                       uint16_t w, uint16_t color);

/** Draw vertical line. Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_vline(framebuffer_t *fb, uint16_t x, uint16_t y,
                       uint16_t h, uint16_t color);

/** Draw rectangle outline. Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);

/** Draw filled rectangle. Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_fill_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint16_t color);

/** Draw arbitrary line (Bresenham). Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_line(framebuffer_t *fb, int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1, uint16_t color);

/** Draw circle outline (midpoint algorithm). Clips to bounds.
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_circle(framebuffer_t *fb, int16_t cx, int16_t cy,
                        uint16_t r, uint16_t color);

/** Blit a 1-bit bitmap into the framebuffer.
 *  Bitmap is row-major, MSB first, each row padded to byte boundary.
 *  Only '1' bits in the bitmap are drawn (transparent background).
 *  @thread_safe: no @isr_safe: yes */
void framebuffer_blit_1bpp(framebuffer_t *fb, uint16_t x, uint16_t y,
                           const uint8_t *bitmap, uint16_t bw, uint16_t bh,
                           uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUFFER_H */
