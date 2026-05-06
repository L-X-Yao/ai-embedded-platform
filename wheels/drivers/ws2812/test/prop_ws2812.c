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
    /* v8 is uint8_t, so always 0-255 by type — check monotonicity instead */
    if (v5 == 0)  PROP_ASSERT(v8 == 0);
    if (v5 == 31) PROP_ASSERT(v8 == 255);
    /* Monotonicity: higher input should give >= output */
    if (v5 > 0) {
        uint8_t prev = ws2812_scale5to8((uint8_t)(v5 - 1));
        PROP_ASSERT(v8 >= prev);
    }
}

/* --- additional mocks for failure injection --- */

static bool mock_fail_set_compare_count_g;
static int  mock_fail_set_compare_at_g;
static int  mock_set_compare_count_g;

static bool mock_set_compare_fail(void *ctx, uint16_t value)
{
    (void)ctx; (void)value;
    mock_set_compare_count_g++;
    if (mock_fail_set_compare_count_g &&
        mock_set_compare_count_g >= mock_fail_set_compare_at_g)
        return false;
    return true;
}

static void mock_wait_noop(void *ctx) { (void)ctx; }
static void mock_enter_noop(void *ctx) { (void)ctx; }
static void mock_exit_noop(void *ctx)  { (void)ctx; }

static const ws2812_hal_t hal_fail_first = {
    .set_compare    = mock_set_compare_fail,
    .wait_period    = mock_wait_noop,
    .enter_critical = mock_enter_noop,
    .exit_critical  = mock_exit_noop,
    .ctx            = NULL,
};

/* HAL with NULL callbacks — triggers the incomplete-HAL guard */
static const ws2812_hal_t hal_null_callbacks = {
    .set_compare    = NULL,
    .wait_period    = NULL,
    .enter_critical = NULL,
    .exit_critical  = NULL,
    .ctx            = NULL,
};

TEST(test_null_hal_callbacks_rejected)
{
    ws2812_color_t c = {0, 0, 0};
    ASSERT_EQ(ws2812_send(&hal_null_callbacks, &prop_timing, &c, 1),
              WS2812_ERR_NULL);
}

TEST(test_null_arg_guards)
{
    ws2812_color_t c = {0, 0, 0};
    ASSERT_EQ(ws2812_send(NULL,       &prop_timing, &c, 1), WS2812_ERR_NULL);
    ASSERT_EQ(ws2812_send(&prop_hal, NULL,          &c, 1), WS2812_ERR_NULL);
    ASSERT_EQ(ws2812_send(&prop_hal, &prop_timing, NULL, 1), WS2812_ERR_NULL);
    ASSERT_EQ(ws2812_send(&prop_hal, &prop_timing, &c,  0), WS2812_ERR_NULL);
}

TEST(test_scale5to8_clamp)
{
    /* v5 > 31 must clamp to LUT[31] = 255 */
    ASSERT_EQ(ws2812_scale5to8(32),  255);
    ASSERT_EQ(ws2812_scale5to8(100), 255);
    ASSERT_EQ(ws2812_scale5to8(255), 255);
}

TEST(test_encode_grb_null_guards)
{
    ws2812_color_t c = {0, 0, 0};
    uint16_t slots[24];
    /* All null-guards should return without crashing */
    ws2812_encode_grb(NULL, &prop_timing, slots);
    ws2812_encode_grb(&c, NULL, slots);
    ws2812_encode_grb(&c, &prop_timing, NULL);
    /* No assertion needed — just exercising the guard paths */
}

TEST(test_reset_latch_fail)
{
    ws2812_color_t c = {255, 0, 0};
    mock_fail_set_compare_count_g = true;
    mock_fail_set_compare_at_g = 1;
    mock_set_compare_count_g = 0;
    ASSERT_EQ(ws2812_send(&hal_fail_first, &prop_timing, &c, 1),
              WS2812_ERR_HAL);
}

TEST(test_hal_fail_first_data_bit)
{
    ws2812_color_t c = {255, 0, 0};
    /* reset latch uses 1 set_compare; first data bit uses the 2nd call */
    mock_fail_set_compare_count_g = true;
    mock_fail_set_compare_at_g = 2;
    mock_set_compare_count_g = 0;
    ASSERT_EQ(ws2812_send(&hal_fail_first, &prop_timing, &c, 1),
              WS2812_ERR_HAL);
}

TEST(test_hal_fail_mid_data)
{
    ws2812_color_t c = {255, 0, 0};
    /* Fail at bit 15 (well inside the inner loop) */
    mock_fail_set_compare_count_g = true;
    mock_fail_set_compare_at_g = 15;
    mock_set_compare_count_g = 0;
    ASSERT_EQ(ws2812_send(&hal_fail_first, &prop_timing, &c, 1),
              WS2812_ERR_HAL);
}

TEST(test_hal_fail_trailing_zero)
{
    ws2812_color_t c = {0, 0, 0};
    /* reset latch=1 call, then 25 data calls (24 bits + trailing 0)
     * fail on the very last one: call 26 */
    mock_fail_set_compare_count_g = true;
    mock_fail_set_compare_at_g = 26;
    mock_set_compare_count_g = 0;
    ASSERT_EQ(ws2812_send(&hal_fail_first, &prop_timing, &c, 1),
              WS2812_ERR_HAL);
}

TEST_MAIN("prop_ws2812")
    RUN(send_no_crash);
    RUN(encode_grb_no_crash);
    RUN(scale5to8_bounded);
    RUN(test_null_hal_callbacks_rejected);
    RUN(test_null_arg_guards);
    RUN(test_scale5to8_clamp);
    RUN(test_encode_grb_null_guards);
    RUN(test_reset_latch_fail);
    RUN(test_hal_fail_first_data_bit);
    RUN(test_hal_fail_mid_data);
    RUN(test_hal_fail_trailing_zero);
TEST_MAIN_END()
