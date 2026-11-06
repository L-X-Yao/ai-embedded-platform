# font_5x7

**Status: candidate**
**Layer: L0 (primitives)**

5x7 pixel bitmap font covering ASCII 0x20..0x7E (95 printable characters).
Column-major encoding: 5 bytes per glyph, each byte is one column, bit 0 = top row.
Font data is const (ROM-able). All functions are read-only and ISR-safe.

## Origin

Proactive build (ADR-011). 5x7 pixel bitmap font covering ASCII 0x20..0x7E (95 printable characters).


## Why self-written?

existing open-source has display components but no standalone minimal bitmap font primitive.
This wheel provides a zero-dependency glyph lookup usable with any framebuffer or display driver.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99 with `<stdint.h>` and `<stddef.h>`.

## API

```c
#include "font_5x7.h"

/* Built-in font instance (const, ROM-able) */
extern const font_5x7_t font_5x7_default;

/* Get pointer to 5-byte glyph data for a character (NULL if out of range) */
const uint8_t *font_5x7_glyph(const font_5x7_t *font, char ch);

/* Get pixel value at (x, y) within a glyph (0 or 1) */
int font_5x7_pixel(const font_5x7_t *font, char ch, int x, int y);

/* Measure string width in pixels: width * len + spacing * (len - 1) */
int font_5x7_text_width(const font_5x7_t *font, const char *text, int spacing);
```

## Usage

```c
#include "font_5x7.h"

/* Draw a character to a framebuffer */
const font_5x7_t *f = &font_5x7_default;
for (int y = 0; y < 7; y++) {
    for (int x = 0; x < 5; x++) {
        if (font_5x7_pixel(f, 'A', x, y)) {
            fb_set_pixel(col + x, row + y, COLOR_WHITE);
        }
    }
}

/* Measure text for centering */
int w = font_5x7_text_width(f, "Hello", 1);  /* 5*5 + 1*4 = 29 px */
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\font_5x7/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_font_5x7
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
