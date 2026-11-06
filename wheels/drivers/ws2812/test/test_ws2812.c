// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <stdlib.h>
#include "ws2812.h"

/* ---------- mock HAL ---------- */

#define MAX_EVENTS 2048

typedef enum { EVT_SET_COMPARE, EVT_WAIT_PERIOD, EVT_ENTER_CRIT, EVT_EXIT_CRIT } mock_evt_type_t;

typedef struct {
    mock_evt_type_t type;
    uint16_t value;
} mock_event_t;

static mock_event_t mock_log[MAX_EVENTS];
static int          mock_log_idx;
static int          mock_in_critical;
static int          mock_set_compare_count;
static int          mock_set_compare_fail_after;

static void mock_reset(void)
{
    memset(mock_log, 0, sizeof(mock_log));
    mock_log_idx = 0;
    mock_in_critical = 0;
    mock_set_compare_count = 0;
    mock_set_compare_fail_after = 0;
}

static bool mock_set_compare(void *ctx, uint16_t value)
{
    (void)ctx;
    mock_set_compare_count++;
    if (mock_set_compare_fail_after > 0 &&
        mock_set_compare_count > mock_set_compare_fail_after)
        return false;
    if (mock_log_idx < MAX_EVENTS) {
        mock_log[mock_log_idx].type  = EVT_SET_COMPARE;
        mock_log[mock_log_idx].value = value;
        mock_log_idx++;
    }
    return true;
}

static void mock_wait_period(void *ctx)
{
    (void)ctx;
    if (mock_log_idx < MAX_EVENTS) {
        mock_log[mock_log_idx].type = EVT_WAIT_PERIOD;
        mock_log_idx++;
    }
}

static void mock_enter_critical(void *ctx)
{
    (void)ctx;
    mock_in_critical = 1;
    if (mock_log_idx < MAX_EVENTS) {
        mock_log[mock_log_idx].type = EVT_ENTER_CRIT;
        mock_log_idx++;
    }
}

static void mock_exit_critical(void *ctx)
{
    (void)ctx;
    mock_in_critical = 0;
    if (mock_log_idx < MAX_EVENTS) {
        mock_log[mock_log_idx].type = EVT_EXIT_CRIT;
        mock_log_idx++;
    }
}

static const ws2812_hal_t mock_hal = {
    .set_compare    = mock_set_compare,
    .wait_period    = mock_wait_period,
    .enter_critical = mock_enter_critical,
    .exit_critical  = mock_exit_critical,
    .ctx            = NULL,
};

static const ws2812_timing_t timing_48mhz = {
    .period_ticks  = 59U,
    .t0h_ticks     = 16U,
    .t1h_ticks     = 31U,
    .reset_periods = 80U,
};

/* ---------- test helpers ---------- */
/* count events of a given type in the mock log */
static int count_events(mock_evt_type_t type)
{
    int n = 0;
    for (int i = 0; i < mock_log_idx; i++) {
        if (mock_log[i].type == type) n++;
    }
    return n;
}

/* extract set_compare values after the critical section entry */
static int extract_data_compares(uint16_t *out, int max)
{
    int n = 0;
    int in_crit = 0;
    for (int i = 0; i < mock_log_idx && n < max; i++) {
        if (mock_log[i].type == EVT_ENTER_CRIT) in_crit = 1;
        if (mock_log[i].type == EVT_EXIT_CRIT)  in_crit = 0;
        if (in_crit && mock_log[i].type == EVT_SET_COMPARE) {
            out[n++] = mock_log[i].value;
        }
    }
    return n;
}

/* ---------- tests ---------- */

TEST(test_null_params)
{
    ws2812_color_t c = {0, 0, 0};
    ws2812_err_t err;
    err = ws2812_send(NULL, &timing_48mhz, &c, 1);
    ASSERT_TRUE(err == WS2812_ERR_NULL);

    err = ws2812_send(&mock_hal, NULL, &c, 1);
    ASSERT_TRUE(err == WS2812_ERR_NULL);

    err = ws2812_send(&mock_hal, &timing_48mhz, NULL, 1);
    ASSERT_TRUE(err == WS2812_ERR_NULL);

    err = ws2812_send(&mock_hal, &timing_48mhz, &c, 0);
    ASSERT_TRUE(err == WS2812_ERR_NULL);
}

TEST(test_single_led_black)
{
    ws2812_color_t c = {0, 0, 0};
    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, &c, 1);
    ASSERT_TRUE(err == WS2812_OK);

    /* 24 data bits + 1 trailing zero = 25 set_compare calls in critical section */
    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT_TRUE(n == 25);

    /* all data bits should be T0H (black = all zeros) */
    for (int i = 0; i < 24; i++) {
        ASSERT_TRUE(vals[i] == timing_48mhz.t0h_ticks);
    }
    ASSERT_TRUE(vals[24] == 0);
}

TEST(test_single_led_white)
{
    ws2812_color_t c = {255, 255, 255};
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT_TRUE(n == 25);

    for (int i = 0; i < 24; i++) {
        ASSERT_TRUE(vals[i] == timing_48mhz.t1h_ticks);
    }
}

TEST(test_grb_order)
{
    /* R=0xFF, G=0x00, B=0x00 should encode as G(00) R(FF) B(00) */
    ws2812_color_t c = {0xFF, 0x00, 0x00};
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT_TRUE(n == 25);

    /* G byte (bits 0-7): all T0H */
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(vals[i] == timing_48mhz.t0h_ticks);
    }
    /* R byte (bits 8-15): all T1H */
    for (int i = 8; i < 16; i++) {
        ASSERT_TRUE(vals[i] == timing_48mhz.t1h_ticks);
    }
    /* B byte (bits 16-23): all T0H */
    for (int i = 16; i < 24; i++) {
        ASSERT_TRUE(vals[i] == timing_48mhz.t0h_ticks);
    }
}

TEST(test_bit_encoding_pattern)
{
    /* R=0, G=0xA5, B=0 → G byte = 10100101 */
    ws2812_color_t c = {0, 0xA5, 0};
    uint16_t slots[24];
    ws2812_encode_grb(&c, &timing_48mhz, slots);

    /* G byte = 0xA5 = 10100101 */
    uint16_t expected[] = {
        timing_48mhz.t1h_ticks, timing_48mhz.t0h_ticks,
        timing_48mhz.t1h_ticks, timing_48mhz.t0h_ticks,
        timing_48mhz.t0h_ticks, timing_48mhz.t1h_ticks,
        timing_48mhz.t0h_ticks, timing_48mhz.t1h_ticks,
    };
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(slots[i] == expected[i]);
    }
    /* R and B all zero */
    for (int i = 8; i < 24; i++) {
        ASSERT_TRUE(slots[i] == timing_48mhz.t0h_ticks);
    }
}

TEST(test_multi_led)
{
    ws2812_color_t colors[3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, colors, 3);
    ASSERT_TRUE(err == WS2812_OK);

    /* 3 LEDs × 24 bits + 1 trailing zero = 73 set_compare in critical section */
    uint16_t vals[128];
    int n = extract_data_compares(vals, 128);
    ASSERT_TRUE(n == 73);
}

TEST(test_reset_latch)
{
    ws2812_color_t c = {0, 0, 0};
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    /* Before critical section, reset latch should produce:
     * 1 set_compare(0) + 80 wait_period calls */
    int set_before_crit = 0;
    int wait_before_crit = 0;
    for (int i = 0; i < mock_log_idx; i++) {
        if (mock_log[i].type == EVT_ENTER_CRIT) break;
        if (mock_log[i].type == EVT_SET_COMPARE) set_before_crit++;
        if (mock_log[i].type == EVT_WAIT_PERIOD) wait_before_crit++;
    }
    ASSERT_TRUE(set_before_crit == 1);
    ASSERT_TRUE(wait_before_crit == (int)timing_48mhz.reset_periods);
}

TEST(test_critical_section)
{
    ws2812_color_t c = {128, 64, 32};
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    ASSERT_TRUE(count_events(EVT_ENTER_CRIT) == 1);
    ASSERT_TRUE(count_events(EVT_EXIT_CRIT)  == 1);

    /* enter must come before exit */
    int enter_idx = -1, exit_idx = -1;
    for (int i = 0; i < mock_log_idx; i++) {
        if (mock_log[i].type == EVT_ENTER_CRIT && enter_idx < 0) enter_idx = i;
        if (mock_log[i].type == EVT_EXIT_CRIT  && exit_idx  < 0) exit_idx  = i;
    }
    ASSERT_TRUE(enter_idx < exit_idx);
}

TEST(test_scale5to8)
{
    ASSERT_TRUE(ws2812_scale5to8(0) == 0);
    ASSERT_TRUE(ws2812_scale5to8(31) == 255);
    ASSERT_TRUE(ws2812_scale5to8(16) == 131);
    ASSERT_TRUE(ws2812_scale5to8(32) == 255);
    ASSERT_TRUE(ws2812_scale5to8(255) == 255);

    /* monotonic */
    for (uint8_t i = 1; i < 32; i++) {
        ASSERT_TRUE(ws2812_scale5to8(i) > ws2812_scale5to8(i - 1));
    }
}

TEST(test_different_timing)
{
    ws2812_timing_t timing_72mhz = {
        .period_ticks  = 89U,
        .t0h_ticks     = 24U,
        .t1h_ticks     = 47U,
        .reset_periods = 80U,
    };
    ws2812_color_t c = {0xFF, 0, 0};
    uint16_t slots[24];
    ws2812_encode_grb(&c, &timing_72mhz, slots);

    /* G=0 → all T0H=24 */
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(slots[i] == 24U);
    }
    /* R=0xFF → all T1H=47 */
    for (int i = 8; i < 16; i++) {
        ASSERT_TRUE(slots[i] == 47U);
    }
}

TEST(test_large_led_count)
{
    ws2812_color_t colors[30];
    for (int i = 0; i < 30; i++) {
        colors[i].r = (uint8_t)(i * 8);
        colors[i].g = (uint8_t)(255 - i * 8);
        colors[i].b = 128;
    }

    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, colors, 30);
    ASSERT_TRUE(err == WS2812_OK);

    uint16_t vals[1024];
    int n = extract_data_compares(vals, 1024);
    ASSERT_TRUE(n == 30 * 24 + 1);
}

TEST(test_all_color_corners)
{
    uint8_t vals[] = {0, 255};
    for (int ri = 0; ri < 2; ri++) {
        for (int gi = 0; gi < 2; gi++) {
            for (int bi = 0; bi < 2; bi++) {
                ws2812_color_t c = { vals[ri], vals[gi], vals[bi] };
                uint16_t slots[24];
                ws2812_encode_grb(&c, &timing_48mhz, slots);

                uint16_t t0 = timing_48mhz.t0h_ticks;
                uint16_t t1 = timing_48mhz.t1h_ticks;
                uint16_t expect_g = gi ? t1 : t0;
                uint16_t expect_r = ri ? t1 : t0;
                uint16_t expect_b = bi ? t1 : t0;

                for (int b = 0; b < 8; b++)
                    ASSERT_TRUE(slots[b] == expect_g);
                for (int b = 8; b < 16; b++)
                    ASSERT_TRUE(slots[b] == expect_r);
                for (int b = 16; b < 24; b++)
                    ASSERT_TRUE(slots[b] == expect_b);
            }
        }
    }
}

TEST(test_mid_gray_encoding)
{
    ws2812_color_t c = {128, 128, 128};
    uint16_t slots[24];
    uint16_t t0 = timing_48mhz.t0h_ticks;
    uint16_t t1 = timing_48mhz.t1h_ticks;
    ws2812_encode_grb(&c, &timing_48mhz, slots);

    /* 128 = 0x80 = 10000000, all three channels identical */
    uint16_t expected_byte[8] = { t1, t0, t0, t0, t0, t0, t0, t0 };
    for (int ch = 0; ch < 3; ch++) {
        for (int b = 0; b < 8; b++) {
            ASSERT_TRUE(slots[ch * 8 + b] == expected_byte[b]);
        }
    }
}

TEST(test_data_inside_critical_section)
{
    ws2812_color_t c = {0xAA, 0x55, 0xCC};
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    int in_crit = 0;
    int data_outside_crit = 0;
    for (int i = 0; i < mock_log_idx; i++) {
        if (mock_log[i].type == EVT_ENTER_CRIT) in_crit = 1;
        if (mock_log[i].type == EVT_EXIT_CRIT)  in_crit = 0;
        if (!in_crit && mock_log[i].type == EVT_SET_COMPARE) {
            /* set_compare(0) for reset latch is ok, only count data values */
            if (mock_log[i].value == timing_48mhz.t0h_ticks ||
                mock_log[i].value == timing_48mhz.t1h_ticks)
                data_outside_crit++;
        }
    }
    ASSERT_TRUE(data_outside_crit == 0);
}

/* A free-running PWM repeats the last compare value in every period that gets
 * no new one, so a batch of 24 slots between two bits adds bits to the stream.
 * The host cannot see time, but it can see when the driver reads a colour: a
 * byte changed during the first bit of LED 2 only goes out if the slots of
 * that LED come from the bit loop and were not all worked out up front. */
static ws2812_color_t late_colors[2];

static bool mock_set_compare_late(void *ctx, uint16_t value)
{
    bool ok = mock_set_compare(ctx, value);
    if (mock_set_compare_count == 26)   /* reset latch, 24 bits, then this */
        late_colors[1].b = 0xFF;
    return ok;
}

TEST(test_slots_derived_in_bit_loop)
{
    const ws2812_hal_t hal = {
        .set_compare    = mock_set_compare_late,
        .wait_period    = mock_wait_period,
        .enter_critical = mock_enter_critical,
        .exit_critical  = mock_exit_critical,
        .ctx            = NULL,
    };
    memset(late_colors, 0, sizeof(late_colors));
    mock_reset();
    ASSERT_TRUE(ws2812_send(&hal, &timing_48mhz, late_colors, 2) == WS2812_OK);

    uint16_t vals[64];
    ASSERT_EQ(extract_data_compares(vals, 64), 49);
    /* LED 1, then G and R of LED 2 */
    for (int i = 0; i < 40; i++)
        ASSERT_EQ(vals[i], timing_48mhz.t0h_ticks);
    /* B of LED 2 as it was when its turn came */
    for (int i = 40; i < 48; i++)
        ASSERT_EQ(vals[i], timing_48mhz.t1h_ticks);
}

/* Every bit waits for exactly one period, also across a change of LED */
TEST(test_one_wait_per_bit)
{
    ws2812_color_t colors[3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mock_reset();
    ASSERT_TRUE(ws2812_send(&mock_hal, &timing_48mhz, colors, 3) == WS2812_OK);

    int i = 0;
    while (mock_log[i].type != EVT_ENTER_CRIT)
        i++;
    i++;
    for (int bit = 0; bit < 3 * 24 + 1; bit++) {
        ASSERT_EQ(mock_log[i].type, EVT_WAIT_PERIOD);
        ASSERT_EQ(mock_log[i + 1].type, EVT_SET_COMPARE);
        i += 2;
    }
    ASSERT_EQ(mock_log[i].type, EVT_EXIT_CRIT);
}

TEST(test_hal_error_during_reset_latch)
{
    ws2812_color_t c = {128, 64, 32};
    mock_reset();
    mock_set_compare_fail_after = 1;
    mock_set_compare_count = 0;
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, &c, 1);
    ASSERT_TRUE(err == WS2812_ERR_HAL);
}

TEST(test_hal_error_during_data)
{
    ws2812_color_t c = {128, 64, 32};
    mock_reset();
    mock_set_compare_fail_after = 5;
    mock_set_compare_count = 0;
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, &c, 1);
    ASSERT_TRUE(err == WS2812_ERR_HAL);
}

/* A separate always-failing set_compare for testing the reset_latch failure
 * path. The main mock's fail_after logic requires fail_after > 0 which
 * prevents triggering on the very first call, so we use a dedicated stub. */
static bool mock_set_compare_always_fail(void *ctx, uint16_t value)
{
    (void)ctx;
    (void)value;
    return false;
}

/* Fail the very first set_compare(0) call inside reset_latch.
 * This exercises reset_latch() "return false" (line 63) and ws2812_send's
 * "!reset_latch → return WS2812_ERR_HAL" branch (line 87). */
TEST(test_hal_error_in_reset_latch_first_call)
{
    ws2812_color_t c = {0, 128, 0};
    ws2812_hal_t hal_fail = {
        .set_compare    = mock_set_compare_always_fail,
        .wait_period    = mock_wait_period,
        .enter_critical = mock_enter_critical,
        .exit_critical  = mock_exit_critical,
        .ctx            = NULL,
    };
    mock_reset();
    ws2812_err_t err = ws2812_send(&hal_fail, &timing_48mhz, &c, 1);
    ASSERT_EQ(err, WS2812_ERR_HAL);
    /* Critical section must NOT have been entered since we failed before it */
    ASSERT_EQ(count_events(EVT_ENTER_CRIT), 0);
}

/* Fail the trailing set_compare(0) that follows the last bit of the last LED.
 * For 1 LED: calls = 1 (reset) + 1 (slot[0]) + 23 (slots[1..23]) + 1 (trailing) = 26.
 * fail_after=25 → call 26 fails → exercises lines 111-112 of ws2812_send. */
TEST(test_hal_error_trailing_zero)
{
    ws2812_color_t c = {255, 255, 255};
    mock_reset();
    mock_set_compare_fail_after = 25;  /* fail on the 26th call (trailing zero) */
    mock_set_compare_count = 0;
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, &c, 1);
    ASSERT_EQ(err, WS2812_ERR_HAL);
    /* Critical section must have been entered and then exited on error */
    ASSERT_EQ(count_events(EVT_ENTER_CRIT), 1);
    ASSERT_EQ(count_events(EVT_EXIT_CRIT),  1);
}

/* Pass a HAL with a NULL function pointer — exercises the second NULL guard
 * (line 82-83 in ws2812_send) that checks individual function pointers. */
TEST(test_null_hal_function_pointer)
{
    ws2812_color_t c = {0, 0, 0};
    ws2812_hal_t hal_missing_fn = {
        .set_compare    = NULL,          /* missing */
        .wait_period    = mock_wait_period,
        .enter_critical = mock_enter_critical,
        .exit_critical  = mock_exit_critical,
        .ctx            = NULL,
    };
    mock_reset();
    ws2812_err_t err = ws2812_send(&hal_missing_fn, &timing_48mhz, &c, 1);
    ASSERT_EQ(err, WS2812_ERR_NULL);

    /* Verify other combinations of missing pointers also return ERR_NULL */
    ws2812_hal_t hal_no_wait = {
        .set_compare    = mock_set_compare,
        .wait_period    = NULL,
        .enter_critical = mock_enter_critical,
        .exit_critical  = mock_exit_critical,
        .ctx            = NULL,
    };
    err = ws2812_send(&hal_no_wait, &timing_48mhz, &c, 1);
    ASSERT_EQ(err, WS2812_ERR_NULL);

    ws2812_hal_t hal_no_crit = {
        .set_compare    = mock_set_compare,
        .wait_period    = mock_wait_period,
        .enter_critical = NULL,
        .exit_critical  = mock_exit_critical,
        .ctx            = NULL,
    };
    err = ws2812_send(&hal_no_crit, &timing_48mhz, &c, 1);
    ASSERT_EQ(err, WS2812_ERR_NULL);
}

/* ws2812_encode_grb with NULL arguments must return without crashing.
 * Exercises the three individual null branches in encode_grb (line 51). */
TEST(test_encode_grb_null_args)
{
    ws2812_color_t c = {1, 2, 3};
    uint16_t slots[24];
    /* Each argument null independently — none should crash */
    ws2812_encode_grb(NULL, &timing_48mhz, slots);
    ws2812_encode_grb(&c, NULL, slots);
    ws2812_encode_grb(&c, &timing_48mhz, NULL);
    ASSERT_TRUE(1);  /* reaching here means no crash */
}

TEST_MAIN("ws2812")
    RUN(test_null_params);
    RUN(test_single_led_black);
    RUN(test_single_led_white);
    RUN(test_grb_order);
    RUN(test_bit_encoding_pattern);
    RUN(test_multi_led);
    RUN(test_reset_latch);
    RUN(test_critical_section);
    RUN(test_scale5to8);
    RUN(test_different_timing);
    RUN(test_large_led_count);
    RUN(test_all_color_corners);
    RUN(test_mid_gray_encoding);
    RUN(test_data_inside_critical_section);
    RUN(test_hal_error_during_reset_latch);
    RUN(test_hal_error_during_data);
    RUN(test_slots_derived_in_bit_loop);
    RUN(test_one_wait_per_bit);
    RUN(test_hal_error_in_reset_latch_first_call);
    RUN(test_hal_error_trailing_zero);
    RUN(test_null_hal_function_pointer);
    RUN(test_encode_grb_null_args);
TEST_MAIN_END()

