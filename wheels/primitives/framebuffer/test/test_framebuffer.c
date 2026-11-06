// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "framebuffer.h"

/* ---- test_init_null ---- */

TEST(test_init_null)
{
    uint8_t buf[128];
    framebuffer_t fb;

    ASSERT_EQ(FRAMEBUFFER_ERR_PARAM,
              framebuffer_init(NULL, buf, 128, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(FRAMEBUFFER_ERR_PARAM,
              framebuffer_init(&fb, NULL, 128, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(FRAMEBUFFER_ERR_PARAM,
              framebuffer_init(&fb, buf, 0, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(FRAMEBUFFER_ERR_PARAM,
              framebuffer_init(&fb, buf, 128, 0, FB_FORMAT_MONO_VLSB));
    /* Invalid format */
    ASSERT_EQ(FRAMEBUFFER_ERR_PARAM,
              framebuffer_init(&fb, buf, 128, 64, (fb_format_t)99));
}

/* ---- test_init_mono_vlsb ---- */

TEST(test_init_mono_vlsb)
{
    /* 128x64 VLSB: stride = 128, buf_size = 128 * 8 = 1024 */
    ASSERT_EQ(1024, framebuffer_buf_size(128, 64, FB_FORMAT_MONO_VLSB));

    /* Non-aligned height: 128x12 => 128 * ceil(12/8) = 128*2 = 256 */
    ASSERT_EQ(256, framebuffer_buf_size(128, 12, FB_FORMAT_MONO_VLSB));

    uint8_t buf[1024];
    framebuffer_t fb;
    memset(buf, 0xAA, sizeof(buf)); /* dirty fill */
    ASSERT_EQ(FRAMEBUFFER_OK,
              framebuffer_init(&fb, buf, 128, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(128, fb.stride);
    ASSERT_EQ(128, fb.width);
    ASSERT_EQ(64, fb.height);
    ASSERT_EQ(FB_FORMAT_MONO_VLSB, fb.format);
    /* Init clears buffer */
    ASSERT_EQ(0, buf[0]);
    ASSERT_EQ(0, buf[1023]);
}

/* ---- test_init_mono_hlsb ---- */

TEST(test_init_mono_hlsb)
{
    /* 128x64 HLSB: stride = ceil(128/8) = 16, buf_size = 16*64 = 1024 */
    ASSERT_EQ(1024, framebuffer_buf_size(128, 64, FB_FORMAT_MONO_HLSB));
    /* Non-aligned width: 12x8 => ceil(12/8)*8 = 2*8 = 16 */
    ASSERT_EQ(16, framebuffer_buf_size(12, 8, FB_FORMAT_MONO_HLSB));

    uint8_t buf[1024];
    framebuffer_t fb;
    ASSERT_EQ(FRAMEBUFFER_OK,
              framebuffer_init(&fb, buf, 128, 64, FB_FORMAT_MONO_HLSB));
    ASSERT_EQ(16, fb.stride);
}

/* ---- test_init_rgb565 ---- */

TEST(test_init_rgb565)
{
    /* 16x16 RGB565: 16*16*2 = 512 */
    ASSERT_EQ(512, framebuffer_buf_size(16, 16, FB_FORMAT_RGB565));

    uint8_t buf[512];
    framebuffer_t fb;
    ASSERT_EQ(FRAMEBUFFER_OK,
              framebuffer_init(&fb, buf, 16, 16, FB_FORMAT_RGB565));
    ASSERT_EQ(32, fb.stride); /* 16 * 2 */
}

/* ---- test_set_get_pixel_mono ---- */

TEST(test_set_get_pixel_mono)
{
    /* VLSB 8x16 */
    uint8_t buf[16]; /* 8 * (16/8) = 16 */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 8, 16, FB_FORMAT_MONO_VLSB);

    /* All pixels start at 0 */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 0, 0));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 7, 15));

    /* Set pixel (0,0) */
    framebuffer_set_pixel(&fb, 0, 0, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 0, 0));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 1, 0));

    /* Set pixel (3,10) */
    framebuffer_set_pixel(&fb, 3, 10, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 3, 10));

    /* Clear pixel (0,0) */
    framebuffer_set_pixel(&fb, 0, 0, 0);
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 0, 0));

    /* HLSB 16x8 */
    uint8_t buf2[16]; /* ceil(16/8)*8 = 16 */
    framebuffer_t fb2;
    framebuffer_init(&fb2, buf2, 16, 8, FB_FORMAT_MONO_HLSB);

    framebuffer_set_pixel(&fb2, 0, 0, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 0, 0));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb2, 1, 0));

    framebuffer_set_pixel(&fb2, 10, 5, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 10, 5));
}

/* ---- test_set_get_pixel_rgb565 ---- */

TEST(test_set_get_pixel_rgb565)
{
    uint8_t buf[32]; /* 4x4*2 = 32 */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 4, 4, FB_FORMAT_RGB565);

    /* Pure red in RGB565: R=31, G=0, B=0 => 0xF800 */
    framebuffer_set_pixel(&fb, 0, 0, 0xF800u);
    ASSERT_EQ_HEX(0xF800u, framebuffer_get_pixel(&fb, 0, 0));

    /* Pure green: 0x07E0 */
    framebuffer_set_pixel(&fb, 1, 1, 0x07E0u);
    ASSERT_EQ_HEX(0x07E0u, framebuffer_get_pixel(&fb, 1, 1));

    /* Pure blue: 0x001F */
    framebuffer_set_pixel(&fb, 2, 2, 0x001Fu);
    ASSERT_EQ_HEX(0x001Fu, framebuffer_get_pixel(&fb, 2, 2));

    /* White: 0xFFFF */
    framebuffer_set_pixel(&fb, 3, 3, 0xFFFFu);
    ASSERT_EQ_HEX(0xFFFFu, framebuffer_get_pixel(&fb, 3, 3));

    /* Verify other pixels untouched */
    ASSERT_EQ_HEX(0x0000u, framebuffer_get_pixel(&fb, 0, 1));
}

/* ---- test_clear ---- */

TEST(test_clear)
{
    /* Mono VLSB: clear to white (all on) */
    uint8_t buf[128]; /* 16x64 VLSB = 16*8 = 128 */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    framebuffer_clear(&fb, 1);
    for (uint16_t y = 0; y < 64; y++) {
        for (uint16_t x = 0; x < 16; x++) {
            ASSERT_EQ(1, framebuffer_get_pixel(&fb, x, y));
        }
    }

    /* Clear to black */
    framebuffer_clear(&fb, 0);
    for (uint16_t y = 0; y < 64; y++) {
        for (uint16_t x = 0; x < 16; x++) {
            ASSERT_EQ(0, framebuffer_get_pixel(&fb, x, y));
        }
    }

    /* RGB565: clear to color 0x1234 */
    uint8_t buf2[32]; /* 4x4*2 = 32 */
    framebuffer_t fb2;
    framebuffer_init(&fb2, buf2, 4, 4, FB_FORMAT_RGB565);

    framebuffer_clear(&fb2, 0x1234u);
    for (uint16_t y = 0; y < 4; y++) {
        for (uint16_t x = 0; x < 4; x++) {
            ASSERT_EQ_HEX(0x1234u, framebuffer_get_pixel(&fb2, x, y));
        }
    }
}

/* ---- test_hline_vline ---- */

TEST(test_hline_vline)
{
    uint8_t buf[128]; /* 16x64 VLSB */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    /* Horizontal line at y=5, x=2..6 (length 5) */
    framebuffer_hline(&fb, 2, 5, 5, 1);
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 1, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 4, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 6, 5));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 7, 5));
    /* Verify no pixels above/below */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 4, 4));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 4, 6));

    framebuffer_clear(&fb, 0);

    /* Vertical line at x=10, y=0..3 (length 4) */
    framebuffer_vline(&fb, 10, 0, 4, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 10, 0));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 10, 1));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 10, 2));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 10, 3));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 10, 4));
    /* Verify no pixels left/right */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 9, 1));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 11, 1));
}

/* ---- test_rect_fill_rect ---- */

TEST(test_rect_fill_rect)
{
    uint8_t buf[128]; /* 16x64 VLSB */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    /* Draw rectangle outline: (2,2) 4x3 */
    framebuffer_rect(&fb, 2, 2, 4, 3, 1);
    /* Top edge: y=2, x=2..5 */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 2));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 2));
    /* Bottom edge: y=4, x=2..5 */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 4));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 4));
    /* Left edge: x=2, y=3 */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 3));
    /* Right edge: x=5, y=3 */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 3));
    /* Interior should be empty */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 3, 3));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 4, 3));

    framebuffer_clear(&fb, 0);

    /* Filled rectangle: (1,1) 3x3 */
    framebuffer_fill_rect(&fb, 1, 1, 3, 3, 1);
    /* All 9 pixels should be set */
    for (uint16_t y = 1; y <= 3; y++) {
        for (uint16_t x = 1; x <= 3; x++) {
            ASSERT_EQ(1, framebuffer_get_pixel(&fb, x, y));
        }
    }
    /* Outside should be clear */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 0, 0));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 4, 1));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 1, 4));
}

/* ---- test_line_bresenham ---- */

TEST(test_line_bresenham)
{
    uint8_t buf[128]; /* 16x64 VLSB */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    /* Diagonal line from (0,0) to (7,7) */
    framebuffer_line(&fb, 0, 0, 7, 7, 1);
    /* Start and end points must be set */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 0, 0));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 7, 7));
    /* Midpoint */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 3, 3));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 4, 4));

    framebuffer_clear(&fb, 0);

    /* Horizontal line via line() */
    framebuffer_line(&fb, 2, 10, 8, 10, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 10));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 10));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 8, 10));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 1, 10));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 9, 10));

    framebuffer_clear(&fb, 0);

    /* Vertical line via line() */
    framebuffer_line(&fb, 5, 0, 5, 6, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 0));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 3));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 6));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 5, 7));

    framebuffer_clear(&fb, 0);

    /* Reverse direction diagonal */
    framebuffer_line(&fb, 7, 7, 0, 0, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 0, 0));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 7, 7));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 4, 4));
}

/* ---- test_line_long_spans ---- */

static uint32_t lit_pixels(const framebuffer_t *fb)
{
    uint32_t n = 0u;
    for (uint16_t y = 0u; y < fb->height; y++) {
        for (uint16_t x = 0u; x < fb->width; x++) {
            if (framebuffer_get_pixel(fb, x, y) != 0u) { n++; }
        }
    }
    return n;
}

/* Every int16_t pair is a legal end point. A span of 2^14 or more wrapped
 * the int16_t error term, so the loop never met the end point and this test
 * hung. Expected values are the ideal line clipped to the 128x64 window:
 * how many pixels land in it and the last of them. */
TEST(test_line_long_spans)
{
    static const struct {
        int16_t  x0, y0, x1, y1;
        uint32_t lit;
        uint16_t last_x, last_y;
    } cases[] = {
        {      0,      0,  16384,     0, 128u, 127u,  0u },
        {      0,      0,  20000,     0, 128u, 127u,  0u },
        {      0,      0,  32767,     0, 128u, 127u,  0u },
        {      0,      0,      0, 16385,  64u,   0u, 63u },
        {      0,      0,      0, 20000,  64u,   0u, 63u },
        {      0,      0, -16384,     0,   1u,   0u,  0u },
        {      0,      0,      0,-16385,   1u,   0u,  0u },
        {      0,      0,  16384,     1, 128u, 127u,  0u },
        {      0,      0,  20000,    10, 128u, 127u,  0u },
        { -20000,      5,  20000,     5, 128u, 127u,  5u },
        {      5, -20000,      7, 20000,  64u,   6u, 63u },
        { -32768,      0,  32767,     0, 128u, 127u,  0u },
        {  32767,      5, -32768,     5, 128u,   0u,  5u },
        { -32768, -32768,  32767, 32767,  64u,  63u, 63u },
    };
    static uint8_t buf[1024]; /* 128x64 VLSB */
    framebuffer_t fb;
    ASSERT_EQ(FRAMEBUFFER_OK,
              framebuffer_init(&fb, buf, 128, 64, FB_FORMAT_MONO_VLSB));

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        framebuffer_clear(&fb, 0);
        framebuffer_line(&fb, cases[i].x0, cases[i].y0,
                         cases[i].x1, cases[i].y1, 1);
        ASSERT_EQ(cases[i].lit, lit_pixels(&fb));
        ASSERT_EQ(1, framebuffer_get_pixel(&fb, cases[i].last_x,
                                           cases[i].last_y));
    }
}

/* ---- test_circle ---- */

TEST(test_circle)
{
    uint8_t buf[128]; /* 16x64 VLSB */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    /* Circle center (8, 32) radius 5 */
    framebuffer_circle(&fb, 8, 32, 5, 1);
    /* Cardinal points: top (8,27), bottom (8,37), left (3,32), right (13,32) */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 8, 27));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 8, 37));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 3, 32));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 13, 32));
    /* Center should not be set (outline only) */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 8, 32));
}

/* ---- test_blit_1bpp ---- */

TEST(test_blit_1bpp)
{
    uint8_t buf[128]; /* 16x64 VLSB */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 16, 64, FB_FORMAT_MONO_VLSB);

    /*
     * 4x3 bitmap (MSB first, row-major, padded to byte):
     * Row 0: 1010 => 0xA0
     * Row 1: 0101 => 0x50
     * Row 2: 1111 => 0xF0
     */
    const uint8_t glyph[] = { 0xA0, 0x50, 0xF0 };

    framebuffer_blit_1bpp(&fb, 2, 4, glyph, 4, 3, 1);

    /* Row 0: pixels (2,4)=1, (3,4)=0, (4,4)=1, (5,4)=0 */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 4));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 3, 4));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 4, 4));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 5, 4));

    /* Row 1: pixels (2,5)=0, (3,5)=1, (4,5)=0, (5,5)=1 */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 2, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 3, 5));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 4, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 5));

    /* Row 2: all 4 pixels set */
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 2, 6));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 3, 6));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 4, 6));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb, 5, 6));

    /* Surrounding pixels should be clear */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 1, 4));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 6, 4));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 2, 3));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb, 2, 7));
}

/* ---- test_clipping ---- */

TEST(test_clipping)
{
    /* Small 8x8 VLSB framebuffer */
    uint8_t buf[8]; /* 8 * (8/8) = 8 */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 8, 8, FB_FORMAT_MONO_VLSB);

    /* Use a guard zone to detect buffer overrun */
    uint8_t guarded[8 + 16];
    memset(guarded, 0xAA, sizeof(guarded));
    memset(guarded + 8, 0xAA, 8); /* 8 guard bytes before */
    framebuffer_t fb2;
    framebuffer_init(&fb2, guarded + 8, 8, 8, FB_FORMAT_MONO_VLSB);
    /* Re-fill guard zones */
    memset(guarded, 0xBB, 8);
    memset(guarded + 8 + 8, 0xBB, 8);

    /* Set pixel outside bounds */
    framebuffer_set_pixel(&fb2, 100, 100, 1);
    framebuffer_set_pixel(&fb2, 8, 0, 1);
    framebuffer_set_pixel(&fb2, 0, 8, 1);

    /* get_pixel outside bounds returns 0 */
    ASSERT_EQ(0, framebuffer_get_pixel(&fb2, 100, 100));
    ASSERT_EQ(0, framebuffer_get_pixel(&fb2, 8, 0));

    /* hline extending past right edge */
    framebuffer_hline(&fb2, 5, 0, 100, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 5, 0));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 7, 0));

    /* vline extending past bottom edge */
    framebuffer_vline(&fb2, 0, 5, 100, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 0, 5));
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 0, 7));

    /* Line that goes partially out of bounds */
    framebuffer_line(&fb2, -5, -5, 3, 3, 1);

    /* Fill rect extending past bounds */
    framebuffer_fill_rect(&fb2, 6, 6, 100, 100, 1);
    ASSERT_EQ(1, framebuffer_get_pixel(&fb2, 7, 7));

    /* Circle at corner -- should not crash */
    framebuffer_circle(&fb2, 0, 0, 10, 1);

    /* Blit extending past bounds */
    const uint8_t big_glyph[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    framebuffer_blit_1bpp(&fb2, 6, 6, big_glyph, 8, 4, 1);

    /* Verify guard zones are intact (no buffer overrun) */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ_HEX(0xBB, guarded[i]);
        ASSERT_EQ_HEX(0xBB, guarded[8 + 8 + i]);
    }
}

/* ---- test_null_safe ---- */

TEST(test_null_safe)
{
    /* All functions should handle NULL gracefully */
    framebuffer_clear(NULL, 0);
    framebuffer_set_pixel(NULL, 0, 0, 1);
    ASSERT_EQ(0, framebuffer_get_pixel(NULL, 0, 0));
    framebuffer_hline(NULL, 0, 0, 10, 1);
    framebuffer_vline(NULL, 0, 0, 10, 1);
    framebuffer_rect(NULL, 0, 0, 10, 10, 1);
    framebuffer_fill_rect(NULL, 0, 0, 10, 10, 1);
    framebuffer_line(NULL, 0, 0, 5, 5, 1);
    framebuffer_circle(NULL, 4, 4, 3, 1);
    framebuffer_blit_1bpp(NULL, 0, 0, (const uint8_t *)"\xFF", 8, 1, 1);
    ASSERT_EQ(0, framebuffer_buf_size(0, 0, FB_FORMAT_MONO_VLSB));
}

/* ---- test_buf_size ---- */

TEST(test_buf_size)
{
    /* VLSB */
    ASSERT_EQ(1024, framebuffer_buf_size(128, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(256, framebuffer_buf_size(128, 12, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(8, framebuffer_buf_size(8, 8, FB_FORMAT_MONO_VLSB));
    /* height=1 => ceil(1/8)=1, so 8*1=8 */
    ASSERT_EQ(8, framebuffer_buf_size(8, 1, FB_FORMAT_MONO_VLSB));

    /* HLSB */
    ASSERT_EQ(1024, framebuffer_buf_size(128, 64, FB_FORMAT_MONO_HLSB));
    ASSERT_EQ(16, framebuffer_buf_size(12, 8, FB_FORMAT_MONO_HLSB));
    ASSERT_EQ(8, framebuffer_buf_size(1, 8, FB_FORMAT_MONO_HLSB));

    /* RGB565 */
    ASSERT_EQ(512, framebuffer_buf_size(16, 16, FB_FORMAT_RGB565));
    ASSERT_EQ(2, framebuffer_buf_size(1, 1, FB_FORMAT_RGB565));

    /* Edge: 0 dimensions */
    ASSERT_EQ(0, framebuffer_buf_size(0, 64, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(0, framebuffer_buf_size(128, 0, FB_FORMAT_MONO_VLSB));
    ASSERT_EQ(0, framebuffer_buf_size(0, 0, FB_FORMAT_RGB565));
}

/* ---- test_blit_1bpp_on_rgb565 ---- */

TEST(test_blit_1bpp_on_rgb565)
{
    uint8_t buf[32]; /* 4x4 RGB565 = 32 */
    framebuffer_t fb;
    framebuffer_init(&fb, buf, 4, 4, FB_FORMAT_RGB565);

    /* 2x2 bitmap: top-left and bottom-right set */
    /* Row 0: 10 => 0x80, Row 1: 01 => 0x40 */
    const uint8_t glyph[] = { 0x80, 0x40 };

    framebuffer_blit_1bpp(&fb, 1, 1, glyph, 2, 2, 0xF800u); /* red */

    ASSERT_EQ_HEX(0xF800u, framebuffer_get_pixel(&fb, 1, 1));
    ASSERT_EQ_HEX(0x0000u, framebuffer_get_pixel(&fb, 2, 1));
    ASSERT_EQ_HEX(0x0000u, framebuffer_get_pixel(&fb, 1, 2));
    ASSERT_EQ_HEX(0xF800u, framebuffer_get_pixel(&fb, 2, 2));
}

/* ---- test_sizes_past_uint16 ---- */

/* Position + size can pass 65535. Each shape is clipped like any other, so the
 * expected counts are what lands in the 128x64 window. */
TEST(test_sizes_past_uint16)
{
    static uint8_t buf[1024];
    framebuffer_t fb;
    ASSERT_EQ(FRAMEBUFFER_OK,
              framebuffer_init(&fb, buf, 128, 64, FB_FORMAT_MONO_VLSB));

    /* 100 + 65436 is exactly 65536: x = 100..127 */
    framebuffer_hline(&fb, 100, 5, 65436, 1);
    ASSERT_EQ(28, lit_pixels(&fb));

    /* y = 10..63 */
    framebuffer_clear(&fb, 0);
    framebuffer_vline(&fb, 5, 10, 65530, 1);
    ASSERT_EQ(54, lit_pixels(&fb));

    framebuffer_clear(&fb, 0);
    framebuffer_fill_rect(&fb, 10, 10, 0xFFFF, 0xFFFF, 1);
    ASSERT_EQ(118 * 54, lit_pixels(&fb));

    /* Bottom edge is off the screen: the top, then 3 rows of each side */
    framebuffer_clear(&fb, 0);
    framebuffer_rect(&fb, 10, 60, 20, 65500, 1);
    ASSERT_EQ(20 + 3 + 3, lit_pixels(&fb));

    /* Right edge is off the screen: top, bottom and 18 rows of the left */
    framebuffer_clear(&fb, 0);
    framebuffer_rect(&fb, 100, 10, 65500, 20, 1);
    ASSERT_EQ(28 + 28 + 18, lit_pixels(&fb));

    /* Starting at the very last coordinate: nothing comes back round */
    framebuffer_clear(&fb, 0);
    framebuffer_rect(&fb, 5, 65535, 10, 5, 1);
    framebuffer_rect(&fb, 65535, 5, 5, 10, 1);
    ASSERT_EQ(0, lit_pixels(&fb));
}

TEST_MAIN("framebuffer")
    RUN(test_init_null);
    RUN(test_init_mono_vlsb);
    RUN(test_init_mono_hlsb);
    RUN(test_init_rgb565);
    RUN(test_set_get_pixel_mono);
    RUN(test_set_get_pixel_rgb565);
    RUN(test_clear);
    RUN(test_hline_vline);
    RUN(test_rect_fill_rect);
    RUN(test_line_bresenham);
    RUN(test_line_long_spans);
    RUN(test_circle);
    RUN(test_blit_1bpp);
    RUN(test_clipping);
    RUN(test_null_safe);
    RUN(test_buf_size);
    RUN(test_blit_1bpp_on_rgb565);
    RUN(test_sizes_past_uint16);
TEST_MAIN_END()
