// SPDX-License-Identifier: MIT
/*
 * framebuffer -- pixel buffer with drawing primitives for mono/RGB565 displays
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "framebuffer.h"
#include <string.h>

/* -------- helpers -------- */

static inline bool fb_valid(const framebuffer_t *fb)
{
    return (fb != NULL) && (fb->buf != NULL) &&
           (fb->width > 0u) && (fb->height > 0u);
}

static inline bool in_bounds(const framebuffer_t *fb, uint16_t x, uint16_t y)
{
    return (x < fb->width) && (y < fb->height);
}

/* Safe set_pixel for signed coords used by Bresenham / circle. */
static inline void sp(framebuffer_t *fb, int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || y < 0) { return; }
    (void)framebuffer_set_pixel(fb, (uint16_t)x, (uint16_t)y, color);
}

static inline int16_t abs16(int16_t v)
{
    return (v < 0) ? (int16_t)(-v) : v;
}

/* -------- buf_size -------- */

uint32_t framebuffer_buf_size(uint16_t width, uint16_t height,
                              fb_format_t format)
{
    if (width == 0u || height == 0u) { return 0u; }
    switch (format) {
    case FB_FORMAT_MONO_VLSB:
        return (uint32_t)width * (uint32_t)((height + 7u) / 8u);
    case FB_FORMAT_MONO_HLSB:
        return (uint32_t)((width + 7u) / 8u) * (uint32_t)height;
    case FB_FORMAT_RGB565:
        return (uint32_t)width * (uint32_t)height * 2u;
    default:
        return 0u;
    }
}

/* -------- init -------- */

framebuffer_err_t framebuffer_init(framebuffer_t *fb, uint8_t *buf,
                                   uint16_t width, uint16_t height,
                                   fb_format_t format)
{
    if (fb == NULL || buf == NULL || width == 0u || height == 0u) {
        return FRAMEBUFFER_ERR_PARAM;
    }
    if (format != FB_FORMAT_MONO_VLSB &&
        format != FB_FORMAT_MONO_HLSB &&
        format != FB_FORMAT_RGB565) {
        return FRAMEBUFFER_ERR_PARAM;
    }

    fb->buf    = buf;
    fb->width  = width;
    fb->height = height;
    fb->format = format;

    switch (format) {
    case FB_FORMAT_MONO_VLSB:
        /* SSD1306: each byte = 8 vertical pixels, stride = width bytes */
        fb->stride = width;
        break;
    case FB_FORMAT_MONO_HLSB:
        /* Each byte = 8 horizontal pixels, stride = ceil(width/8) bytes */
        fb->stride = (uint16_t)((width + 7u) / 8u);
        break;
    case FB_FORMAT_RGB565:
        if (width > 32767u) { return FRAMEBUFFER_ERR_PARAM; }
        fb->stride = (uint16_t)(width * 2u);
        break;
    default:
        return FRAMEBUFFER_ERR_PARAM; /* unreachable after check above */
    }

    uint32_t bsz = framebuffer_buf_size(width, height, format);
    memset(buf, 0, (size_t)bsz);
    return FRAMEBUFFER_OK;
}

/* -------- clear -------- */

void framebuffer_clear(framebuffer_t *fb, uint16_t color)
{
    if (!fb_valid(fb)) { return; }

    uint32_t bsz = framebuffer_buf_size(fb->width, fb->height, fb->format);

    if (fb->format == FB_FORMAT_RGB565) {
        /* Fill with 16-bit color, big-endian */
        uint8_t hi = (uint8_t)(color >> 8);
        uint8_t lo = (uint8_t)(color & 0xFFu);
        for (uint32_t i = 0; i < bsz; i += 2u) {
            fb->buf[i]     = hi;
            fb->buf[i + 1u] = lo;
        }
    } else {
        /* Mono: 0 = all off, nonzero = all on */
        memset(fb->buf, (color != 0u) ? 0xFF : 0x00, (size_t)bsz);
    }
}

/* -------- set_pixel -------- */

void framebuffer_set_pixel(framebuffer_t *fb, uint16_t x, uint16_t y,
                           uint16_t color)
{
    if (!fb_valid(fb) || !in_bounds(fb, x, y)) { return; }

    switch (fb->format) {
    case FB_FORMAT_MONO_VLSB: {
        /* byte index = x + (y / 8) * stride, bit = y & 7, LSB at top */
        uint32_t idx = (uint32_t)x + (uint32_t)(y / 8u) * fb->stride;
        uint8_t mask = (uint8_t)(1u << (y & 7u));
        if (color != 0u) {
            fb->buf[idx] |= mask;
        } else {
            fb->buf[idx] &= (uint8_t)~mask;
        }
        break;
    }
    case FB_FORMAT_MONO_HLSB: {
        /* byte index = (x / 8) + y * stride, bit = x & 7, LSB first */
        uint32_t idx = (uint32_t)(x / 8u) + (uint32_t)y * fb->stride;
        uint8_t mask = (uint8_t)(1u << (x & 7u));
        if (color != 0u) {
            fb->buf[idx] |= mask;
        } else {
            fb->buf[idx] &= (uint8_t)~mask;
        }
        break;
    }
    case FB_FORMAT_RGB565: {
        /* 2 bytes per pixel, big-endian */
        uint32_t idx = ((uint32_t)y * fb->stride) + ((uint32_t)x * 2u);
        fb->buf[idx]      = (uint8_t)(color >> 8);
        fb->buf[idx + 1u] = (uint8_t)(color & 0xFFu);
        break;
    }
    default:
        break;
    }
}

/* -------- get_pixel -------- */

uint16_t framebuffer_get_pixel(const framebuffer_t *fb, uint16_t x,
                               uint16_t y)
{
    if (!fb_valid(fb) || !in_bounds(fb, x, y)) { return 0u; }

    switch (fb->format) {
    case FB_FORMAT_MONO_VLSB: {
        uint32_t idx = (uint32_t)x + (uint32_t)(y / 8u) * fb->stride;
        return (fb->buf[idx] & (uint8_t)(1u << (y & 7u))) != 0u ? 1u : 0u;
    }
    case FB_FORMAT_MONO_HLSB: {
        uint32_t idx = (uint32_t)(x / 8u) + (uint32_t)y * fb->stride;
        return (fb->buf[idx] & (uint8_t)(1u << (x & 7u))) != 0u ? 1u : 0u;
    }
    case FB_FORMAT_RGB565: {
        uint32_t idx = ((uint32_t)y * fb->stride) + ((uint32_t)x * 2u);
        return (uint16_t)(((uint16_t)fb->buf[idx] << 8) |
               (uint16_t)fb->buf[idx + 1u]);
    }
    default:
        return 0u;
    }
}

/* -------- hline -------- */

void framebuffer_hline(framebuffer_t *fb, uint16_t x, uint16_t y,
                       uint16_t w, uint16_t color)
{
    if (!fb_valid(fb) || y >= fb->height || w == 0u) { return; }
    /* Clip x range */
    if (x >= fb->width) { return; }
    uint16_t x_end = x + w;
    if (x_end > fb->width) { x_end = fb->width; }
    for (uint16_t xi = x; xi < x_end; xi++) {
        (void)framebuffer_set_pixel(fb, xi, y, color);
    }
}

/* -------- vline -------- */

void framebuffer_vline(framebuffer_t *fb, uint16_t x, uint16_t y,
                       uint16_t h, uint16_t color)
{
    if (!fb_valid(fb) || x >= fb->width || h == 0u) { return; }
    /* Clip y range */
    if (y >= fb->height) { return; }
    uint16_t y_end = y + h;
    if (y_end > fb->height) { y_end = fb->height; }
    for (uint16_t yi = y; yi < y_end; yi++) {
        (void)framebuffer_set_pixel(fb, x, yi, color);
    }
}

/* -------- rect -------- */

void framebuffer_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color)
{
    if (!fb_valid(fb) || w == 0u || h == 0u) { return; }
    /* Top and bottom horizontal lines */
    (void)framebuffer_hline(fb, x, y, w, color);
    if (h > 1u) {
        (void)framebuffer_hline(fb, x, (uint16_t)(y + h - 1u), w, color);
    }
    /* Left and right vertical lines (skip corners already drawn) */
    if (h > 2u) {
        (void)framebuffer_vline(fb, x, (uint16_t)(y + 1u),
                          (uint16_t)(h - 2u), color);
        if (w > 1u) {
            (void)framebuffer_vline(fb, (uint16_t)(x + w - 1u),
                              (uint16_t)(y + 1u),
                              (uint16_t)(h - 2u), color);
        }
    }
}

/* -------- fill_rect -------- */

void framebuffer_fill_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint16_t color)
{
    if (!fb_valid(fb) || w == 0u || h == 0u) { return; }
    /* Clip */
    if (x >= fb->width || y >= fb->height) { return; }
    uint16_t x_end = x + w;
    uint16_t y_end = y + h;
    if (x_end > fb->width)  { x_end = fb->width; }
    if (y_end > fb->height) { y_end = fb->height; }
    for (uint16_t yi = y; yi < y_end; yi++) {
        for (uint16_t xi = x; xi < x_end; xi++) {
            (void)framebuffer_set_pixel(fb, xi, yi, color);
        }
    }
}

/* -------- line (Bresenham) -------- */

void framebuffer_line(framebuffer_t *fb, int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1, uint16_t color)
{
    if (!fb_valid(fb)) { return; }

    int16_t dx = abs16((int16_t)(x1 - x0));
    int16_t dy = (int16_t)(-abs16((int16_t)(y1 - y0)));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);

    for (;;) {
        sp(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) { break; }
        int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy) {
            err = (int16_t)(err + dy);
            x0  = (int16_t)(x0 + sx);
        }
        if (e2 <= dx) {
            err = (int16_t)(err + dx);
            y0  = (int16_t)(y0 + sy);
        }
    }
}

/* -------- circle (midpoint) -------- */

void framebuffer_circle(framebuffer_t *fb, int16_t cx, int16_t cy,
                        uint16_t r, uint16_t color)
{
    if (!fb_valid(fb) || r == 0u) { return; }

    int16_t x = (int16_t)r;
    int16_t y = 0;
    int16_t d = (int16_t)(1 - (int16_t)r);

    while (x >= y) {
        sp(fb, (int16_t)(cx + x), (int16_t)(cy + y), color);
        sp(fb, (int16_t)(cx - x), (int16_t)(cy + y), color);
        sp(fb, (int16_t)(cx + x), (int16_t)(cy - y), color);
        sp(fb, (int16_t)(cx - x), (int16_t)(cy - y), color);
        sp(fb, (int16_t)(cx + y), (int16_t)(cy + x), color);
        sp(fb, (int16_t)(cx - y), (int16_t)(cy + x), color);
        sp(fb, (int16_t)(cx + y), (int16_t)(cy - x), color);
        sp(fb, (int16_t)(cx - y), (int16_t)(cy - x), color);

        y = (int16_t)(y + 1);
        if (d <= 0) {
            d = (int16_t)(d + 2 * y + 1);
        } else {
            x = (int16_t)(x - 1);
            d = (int16_t)(d + 2 * (y - x) + 1);
        }
    }
}

/* -------- blit_1bpp -------- */

void framebuffer_blit_1bpp(framebuffer_t *fb, uint16_t x, uint16_t y,
                           const uint8_t *bitmap, uint16_t bw, uint16_t bh,
                           uint16_t color)
{
    if (!fb_valid(fb) || bitmap == NULL || bw == 0u || bh == 0u) { return; }

    uint16_t row_bytes = (uint16_t)((bw + 7u) / 8u);

    for (uint16_t row = 0u; row < bh; row++) {
        uint16_t py = (uint16_t)(y + row);
        if (py >= fb->height) { break; }
        for (uint16_t col = 0u; col < bw; col++) {
            uint16_t px = (uint16_t)(x + col);
            if (px >= fb->width) { break; }
            uint16_t byte_idx = (uint16_t)(row * row_bytes + col / 8u);
            uint8_t bit_mask = (uint8_t)(0x80u >> (col & 7u));
            if ((bitmap[byte_idx] & bit_mask) != 0u) {
                (void)framebuffer_set_pixel(fb, px, py, color);
            }
        }
    }
}
