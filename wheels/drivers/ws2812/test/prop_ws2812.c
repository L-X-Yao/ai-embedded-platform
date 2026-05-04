// SPDX-License-Identifier: MIT
/*
 * prop_ws2812.c — property-based tests for WS2812 addressable LED driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "ws2812.h"

/* --- PWM HAL mock --- */

static int mock_compare_count;
static int mock_period_count;

static bool mock_set_compare(void *ctx, uint16_t value)
{
    (void)ctx; (void)value;
    mock_compare_count++;
    return true;
}
static void mock_wait_period(void *ctx) { (void)ctx; mock_period_count++; }
static void mock_enter_critical(void *ctx) { (void)ctx; }
static void mock_exit_critical(void *ctx)  { (void)ctx; }

static const ws2812_hal_t prop_hal = {
    .set_compare    = mock_set_compare,
    .wait_period    = mock_wait_period,
    .enter_critical = mock_enter_critical,
    .exit_critical  = mock_exit_critical,
    .ctx            = NULL,
};

static const ws2812_timing_t prop_timing = {
    .period_ticks  = 1250,
    .t0h_ticks     = 350,
    .t1h_ticks     = 900,
    .reset_periods = 50,
};

PROPERTY(send_no_crash, 5000)
{
    mock_compare_count = 0;
    mock_period_count = 0;

    uint32_t num = 1 + ARBI_UINT8(seed) % 8;
    ws2812_color_t colors[8];
    for (uint32_t i = 0; i < num; i++) {
        colors[i].r = ARBI_UINT8(seed);
        colors[i].g = ARBI_UINT8(seed);
        colors[i].b = ARBI_UINT8(seed);
    }

    ws2812_send(&prop_hal, &prop_timing, colors, num);
}

PROPERTY(encode_grb_no_crash, 10000)
{
    ws2812_color_t color = {
        .r = ARBI_UINT8(seed),
        .g = ARBI_UINT8(seed),
        .b = ARBI_UINT8(seed),
    };
    uint16_t slots[24];
    ws2812_encode_grb(&color, &prop_timing, slots);

    /* Each slot must be either t0h or t1h */
    for (int i = 0; i < 24; i++) {
        PROP_ASSERT(slots[i] == prop_timing.t0h_ticks ||
                    slots[i] == prop_timing.t1h_ticks);
    }
}

PROPERTY(scale5to8_bounded, 5000)
{
    uint8_t v5 = ARBI_UINT8(seed) % 32;
    uint8_t v8 = ws2812_scale5to8(v5);
    PROP_ASSERT(v8 <= 255);
    if (v5 == 0)  PROP_ASSERT(v8 == 0);
    if (v5 == 31) PROP_ASSERT(v8 == 255);
}

TEST_MAIN("prop_ws2812")
    RUN(send_no_crash);
    RUN(encode_grb_no_crash);
    RUN(scale5to8_bounded);
TEST_MAIN_END()
