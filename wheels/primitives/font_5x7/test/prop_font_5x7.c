// SPDX-License-Identifier: MIT
/*
 * prop_font_5x7.c --- property-based tests for font_5x7
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_font_5x7.c ../font_5x7.c -o prop_font_5x7
 */

#include "test_harness.h"
#include "property_test.h"
#include "../font_5x7.h"
#include <string.h>

/* Property: glyph lookup is deterministic -- same char always returns same data */
PROPERTY(glyph_deterministic, 10000) {
    char ch = (char)(0x20 + ARBI_SIZE(seed, 0x5E)); /* 0x20..0x7E */

    const uint8_t *g1 = font_5x7_glyph(&font_5x7_default, ch);
    const uint8_t *g2 = font_5x7_glyph(&font_5x7_default, ch);

    PROP_ASSERT(g1 != NULL);
    PROP_ASSERT(g2 != NULL);
    PROP_ASSERT(g1 == g2); /* same pointer */
    PROP_ASSERT_MEM_EQ(g1, g2, 5);
}

/* Property: pixel values are bounded (0 or 1) */
PROPERTY(pixel_bounds, 10000) {
    char ch = (char)(0x20 + ARBI_SIZE(seed, 0x5E));
    int x = ARBI_RANGE(seed, 0, 4);
    int y = ARBI_RANGE(seed, 0, 6);

    int px = font_5x7_pixel(&font_5x7_default, ch, x, y);
    PROP_ASSERT(px == 0 || px == 1);
}

/* Property: all printable ASCII chars have non-NULL glyphs */
PROPERTY(all_printable_have_glyphs, 1) {
    for (int c = 0x20; c <= 0x7E; c++) {
        const uint8_t *g = font_5x7_glyph(&font_5x7_default, (char)c);
        PROP_ASSERT(g != NULL);
    }
}

/* Property: out-of-range chars return NULL */
PROPERTY(out_of_range_returns_null, 10000) {
    /* Generate chars outside 0x20..0x7E */
    uint8_t raw = ARBI_UINT8(seed);
    if (raw >= 0x20 && raw <= 0x7E) return; /* skip valid range */

    const uint8_t *g = font_5x7_glyph(&font_5x7_default, (char)raw);
    PROP_ASSERT(g == NULL);
}

/* Property: out-of-bounds pixel coords return 0 */
PROPERTY(oob_pixel_returns_zero, 5000) {
    char ch = (char)(0x20 + ARBI_SIZE(seed, 0x5E));

    /* x out of bounds */
    int px1 = font_5x7_pixel(&font_5x7_default, ch, -1, 0);
    PROP_ASSERT_EQ(px1, 0);
    int px2 = font_5x7_pixel(&font_5x7_default, ch, 5, 0);
    PROP_ASSERT_EQ(px2, 0);

    /* y out of bounds */
    int px3 = font_5x7_pixel(&font_5x7_default, ch, 0, -1);
    PROP_ASSERT_EQ(px3, 0);
    int px4 = font_5x7_pixel(&font_5x7_default, ch, 0, 7);
    PROP_ASSERT_EQ(px4, 0);
}

/* Property: text_width is non-negative and consistent */
PROPERTY(text_width_consistent, 5000) {
    /* Generate a short string of printable chars */
    char text[17];
    size_t len = ARBI_SIZE(seed, 15) + 1;
    for (size_t i = 0; i < len; i++) {
        text[i] = (char)(0x20 + ARBI_SIZE(seed, 0x5E));
    }
    text[len] = '\0';

    int spacing = ARBI_RANGE(seed, 0, 3);
    int w = font_5x7_text_width(&font_5x7_default, text, spacing);
    int expected = (int)len * 5 + (int)(len - 1) * spacing;
    PROP_ASSERT_EQ(w, expected);
}

TEST_MAIN("font_5x7 properties") {
    RUN(glyph_deterministic);
    RUN(pixel_bounds);
    RUN(all_printable_have_glyphs);
    RUN(out_of_range_returns_null);
    RUN(oob_pixel_returns_zero);
    RUN(text_width_consistent);
}
TEST_MAIN_END()
