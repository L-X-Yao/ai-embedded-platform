// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "font_5x7.h"

/* --- default font instance --- */

TEST(test_default_font_properties)
{
    ASSERT_EQ(font_5x7_default.width, 5);
    ASSERT_EQ(font_5x7_default.height, 7);
    ASSERT_EQ(font_5x7_default.first, 0x20);
    ASSERT_EQ(font_5x7_default.last, 0x7E);
    ASSERT_NOT_NULL(font_5x7_default.data);
}

/* --- glyph lookup --- */

TEST(test_glyph_space)
{
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, ' ');
    ASSERT_NOT_NULL(g);
    /* Space should be all zeros */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(g[i], 0x00);
    }
}

TEST(test_glyph_A)
{
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, 'A');
    ASSERT_NOT_NULL(g);
    /* 'A' = { 0x7E, 0x11, 0x11, 0x11, 0x7E } */
    ASSERT_EQ_HEX(g[0], 0x7E);
    ASSERT_EQ_HEX(g[1], 0x11);
    ASSERT_EQ_HEX(g[2], 0x11);
    ASSERT_EQ_HEX(g[3], 0x11);
    ASSERT_EQ_HEX(g[4], 0x7E);
}

TEST(test_glyph_zero)
{
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, '0');
    ASSERT_NOT_NULL(g);
    /* '0' = { 0x3E, 0x51, 0x49, 0x45, 0x3E } */
    ASSERT_EQ_HEX(g[0], 0x3E);
    ASSERT_EQ_HEX(g[1], 0x51);
    ASSERT_EQ_HEX(g[2], 0x49);
    ASSERT_EQ_HEX(g[3], 0x45);
    ASSERT_EQ_HEX(g[4], 0x3E);
}

TEST(test_glyph_tilde)
{
    /* Last printable char (0x7E) */
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, '~');
    ASSERT_NOT_NULL(g);
}

TEST(test_glyph_all_printable)
{
    /* All printable chars should return non-NULL */
    for (int c = 0x20; c <= 0x7E; c++) {
        const uint8_t *g = font_5x7_glyph(&font_5x7_default, (char)c);
        ASSERT_NOT_NULL(g);
    }
}

/* --- out-of-range --- */

TEST(test_glyph_below_range)
{
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, '\x1F');
    ASSERT_TRUE(g == NULL);
}

TEST(test_glyph_above_range)
{
    const uint8_t *g = font_5x7_glyph(&font_5x7_default, '\x7F');
    ASSERT_TRUE(g == NULL);
}

TEST(test_glyph_null_font)
{
    const uint8_t *g = font_5x7_glyph(NULL, 'A');
    ASSERT_TRUE(g == NULL);
}

/* --- pixel access --- */

TEST(test_pixel_A_pattern)
{
    /* 'A' column 0 = 0x7E = 0b01111110
     * Bit 0 (top row)  = 0
     * Bit 1           = 1
     * Bit 6 (bottom)  = 1
     */
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, 0), 0); /* bit 0 */
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, 1), 1); /* bit 1 */
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, 2), 1); /* bit 2 */
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, 6), 1); /* bit 6 */

    /* 'A' column 1 = 0x11 = 0b00010001
     * Bit 0 = 1, bit 4 = 1, rest = 0
     */
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 1, 0), 1);
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 1, 1), 0);
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 1, 4), 1);
}

TEST(test_pixel_space_all_zero)
{
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 7; y++) {
            ASSERT_EQ(font_5x7_pixel(&font_5x7_default, ' ', x, y), 0);
        }
    }
}

TEST(test_pixel_out_of_bounds)
{
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', -1, 0), 0);
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 5, 0), 0);
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, -1), 0);
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, 'A', 0, 7), 0);
}

TEST(test_pixel_out_of_range_char)
{
    ASSERT_EQ(font_5x7_pixel(&font_5x7_default, '\x7F', 0, 0), 0);
}

TEST(test_pixel_null_font)
{
    ASSERT_EQ(font_5x7_pixel(NULL, 'A', 0, 0), 0);
}

/* --- text width --- */

TEST(test_text_width_single_char)
{
    /* width=5, spacing irrelevant for single char */
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, "A", 1), 5);
}

TEST(test_text_width_hello)
{
    /* "Hello" = 5 chars, width=5, spacing=1 -> 5*5 + 4*1 = 29 */
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, "Hello", 1), 29);
}

TEST(test_text_width_zero_spacing)
{
    /* "Hi" = 2 chars, width=5, spacing=0 -> 2*5 + 1*0 = 10 */
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, "Hi", 0), 10);
}

TEST(test_text_width_negative_spacing)
{
    /* Negative spacing (overlapping chars) should work mathematically
     * "AB" = 2 chars, width=5, spacing=-1 -> 2*5 + 1*(-1) = 9 */
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, "AB", -1), 9);
}

TEST(test_text_width_empty_string)
{
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, "", 1), 0);
}

TEST(test_text_width_null_text)
{
    ASSERT_EQ(font_5x7_text_width(&font_5x7_default, NULL, 1), 0);
}

TEST(test_text_width_null_font)
{
    ASSERT_EQ(font_5x7_text_width(NULL, "Hello", 1), 0);
}

TEST_MAIN("font_5x7")
    RUN(test_default_font_properties);
    RUN(test_glyph_space);
    RUN(test_glyph_A);
    RUN(test_glyph_zero);
    RUN(test_glyph_tilde);
    RUN(test_glyph_all_printable);
    RUN(test_glyph_below_range);
    RUN(test_glyph_above_range);
    RUN(test_glyph_null_font);
    RUN(test_pixel_A_pattern);
    RUN(test_pixel_space_all_zero);
    RUN(test_pixel_out_of_bounds);
    RUN(test_pixel_out_of_range_char);
    RUN(test_pixel_null_font);
    RUN(test_text_width_single_char);
    RUN(test_text_width_hello);
    RUN(test_text_width_zero_spacing);
    RUN(test_text_width_negative_spacing);
    RUN(test_text_width_empty_string);
    RUN(test_text_width_null_text);
    RUN(test_text_width_null_font);
TEST_MAIN_END()
