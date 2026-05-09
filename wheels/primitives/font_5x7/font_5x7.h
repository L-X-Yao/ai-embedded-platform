// SPDX-License-Identifier: MIT
/*
 * font_5x7 --- bitmap font for embedded displays
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * 5x7 pixel bitmap font covering ASCII 0x20..0x7E (95 printable chars).
 * Column-major encoding: 5 bytes per glyph, each byte is one column,
 * bit 0 = top row.  Font data is const (ROM-able).
 * All functions are reentrant and ISR-safe (read-only access).
 */

#ifndef FONT_5X7_H
#define FONT_5X7_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- types ---------- */

typedef struct {
    uint8_t        width;   /* glyph width in pixels  (5) */
    uint8_t        height;  /* glyph height in pixels (7) */
    uint8_t        first;   /* first character code (0x20) */
    uint8_t        last;    /* last character code  (0x7E) */
    const uint8_t *data;    /* glyph data: 5 bytes per char, column-major */
} font_5x7_t;

/** Default built-in 5x7 font (ROM-able). */
extern const font_5x7_t font_5x7_default;

/* ---------- API ---------- */

/** Get pointer to 5-byte glyph data for a character.
 *  Returns NULL if ch is out of range or font is NULL.
 *  @thread_safe: yes @isr_safe: yes */
const uint8_t *font_5x7_glyph(const font_5x7_t *font, char ch);

/** Get pixel value at (x, y) within a glyph.
 *  x: column (0..width-1), y: row (0..height-1).
 *  Returns 0 or 1; returns 0 if out of range or font is NULL.
 *  @thread_safe: yes @isr_safe: yes */
int font_5x7_pixel(const font_5x7_t *font, char ch, int x, int y);

/** Measure string width in pixels: width * len + spacing * (len - 1).
 *  Returns 0 for NULL or empty string.
 *  @thread_safe: yes @isr_safe: yes */
int font_5x7_text_width(const font_5x7_t *font, const char *text, int spacing);

#ifdef __cplusplus
}
#endif

#endif /* FONT_5X7_H */
