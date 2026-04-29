#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../ws2812.h"

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

static void mock_reset(void)
{
    memset(mock_log, 0, sizeof(mock_log));
    mock_log_idx = 0;
    mock_in_critical = 0;
}

static void mock_set_compare(void *ctx, uint16_t value)
{
    (void)ctx;
    if (mock_log_idx < MAX_EVENTS) {
        mock_log[mock_log_idx].type  = EVT_SET_COMPARE;
        mock_log[mock_log_idx].value = value;
        mock_log_idx++;
    }
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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

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

static void test_null_params(void)
{
    ws2812_color_t c = {0, 0, 0};
    ws2812_err_t err;

    printf("test_null_params\n");
    err = ws2812_send(NULL, &timing_48mhz, &c, 1);
    ASSERT(err == WS2812_ERR_NULL, "null hal");

    err = ws2812_send(&mock_hal, NULL, &c, 1);
    ASSERT(err == WS2812_ERR_NULL, "null timing");

    err = ws2812_send(&mock_hal, &timing_48mhz, NULL, 1);
    ASSERT(err == WS2812_ERR_NULL, "null colors");

    err = ws2812_send(&mock_hal, &timing_48mhz, &c, 0);
    ASSERT(err == WS2812_ERR_NULL, "zero leds");
}

static void test_single_led_black(void)
{
    ws2812_color_t c = {0, 0, 0};

    printf("test_single_led_black\n");
    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, &c, 1);
    ASSERT(err == WS2812_OK, "send ok");

    /* 24 data bits + 1 trailing zero = 25 set_compare calls in critical section */
    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT(n == 25, "25 compares in critical section");

    /* all data bits should be T0H (black = all zeros) */
    for (int i = 0; i < 24; i++) {
        ASSERT(vals[i] == timing_48mhz.t0h_ticks, "bit is T0H for black");
    }
    ASSERT(vals[24] == 0, "trailing zero");
}

static void test_single_led_white(void)
{
    ws2812_color_t c = {255, 255, 255};

    printf("test_single_led_white\n");
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT(n == 25, "25 compares");

    for (int i = 0; i < 24; i++) {
        ASSERT(vals[i] == timing_48mhz.t1h_ticks, "bit is T1H for white");
    }
}

static void test_grb_order(void)
{
    /* R=0xFF, G=0x00, B=0x00 should encode as G(00) R(FF) B(00) */
    ws2812_color_t c = {0xFF, 0x00, 0x00};

    printf("test_grb_order\n");
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    uint16_t vals[64];
    int n = extract_data_compares(vals, 64);
    ASSERT(n == 25, "25 compares");

    /* G byte (bits 0-7): all T0H */
    for (int i = 0; i < 8; i++) {
        ASSERT(vals[i] == timing_48mhz.t0h_ticks, "G byte all zero");
    }
    /* R byte (bits 8-15): all T1H */
    for (int i = 8; i < 16; i++) {
        ASSERT(vals[i] == timing_48mhz.t1h_ticks, "R byte all one");
    }
    /* B byte (bits 16-23): all T0H */
    for (int i = 16; i < 24; i++) {
        ASSERT(vals[i] == timing_48mhz.t0h_ticks, "B byte all zero");
    }
}

static void test_bit_encoding_pattern(void)
{
    /* R=0, G=0xA5, B=0 → G byte = 10100101 */
    ws2812_color_t c = {0, 0xA5, 0};
    uint16_t slots[24];

    printf("test_bit_encoding_pattern\n");
    ws2812_encode_grb(&c, &timing_48mhz, slots);

    /* G byte = 0xA5 = 10100101 */
    uint16_t expected[] = {
        timing_48mhz.t1h_ticks, timing_48mhz.t0h_ticks,
        timing_48mhz.t1h_ticks, timing_48mhz.t0h_ticks,
        timing_48mhz.t0h_ticks, timing_48mhz.t1h_ticks,
        timing_48mhz.t0h_ticks, timing_48mhz.t1h_ticks,
    };
    for (int i = 0; i < 8; i++) {
        ASSERT(slots[i] == expected[i], "G byte bit pattern");
    }
    /* R and B all zero */
    for (int i = 8; i < 24; i++) {
        ASSERT(slots[i] == timing_48mhz.t0h_ticks, "R/B zero");
    }
}

static void test_multi_led(void)
{
    ws2812_color_t colors[3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};

    printf("test_multi_led\n");
    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, colors, 3);
    ASSERT(err == WS2812_OK, "send 3 leds ok");

    /* 3 LEDs × 24 bits + 1 trailing zero = 73 set_compare in critical section */
    uint16_t vals[128];
    int n = extract_data_compares(vals, 128);
    ASSERT(n == 73, "73 compares for 3 LEDs");
}

static void test_reset_latch(void)
{
    ws2812_color_t c = {0, 0, 0};

    printf("test_reset_latch\n");
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
    ASSERT(set_before_crit == 1, "1 set_compare before critical");
    ASSERT(wait_before_crit == (int)timing_48mhz.reset_periods,
           "reset_periods waits before critical");
}

static void test_critical_section(void)
{
    ws2812_color_t c = {128, 64, 32};

    printf("test_critical_section\n");
    mock_reset();
    ws2812_send(&mock_hal, &timing_48mhz, &c, 1);

    ASSERT(count_events(EVT_ENTER_CRIT) == 1, "one enter_critical");
    ASSERT(count_events(EVT_EXIT_CRIT)  == 1, "one exit_critical");

    /* enter must come before exit */
    int enter_idx = -1, exit_idx = -1;
    for (int i = 0; i < mock_log_idx; i++) {
        if (mock_log[i].type == EVT_ENTER_CRIT && enter_idx < 0) enter_idx = i;
        if (mock_log[i].type == EVT_EXIT_CRIT  && exit_idx  < 0) exit_idx  = i;
    }
    ASSERT(enter_idx < exit_idx, "enter before exit");
}

static void test_scale5to8(void)
{
    printf("test_scale5to8\n");
    ASSERT(ws2812_scale5to8(0) == 0, "scale 0 → 0");
    ASSERT(ws2812_scale5to8(31) == 255, "scale 31 → 255");
    ASSERT(ws2812_scale5to8(16) == 131, "scale 16 → 131");
    ASSERT(ws2812_scale5to8(32) == 255, "clamp 32 → 255");
    ASSERT(ws2812_scale5to8(255) == 255, "clamp 255 → 255");

    /* monotonic */
    for (uint8_t i = 1; i < 32; i++) {
        ASSERT(ws2812_scale5to8(i) > ws2812_scale5to8(i - 1), "monotonic");
    }
}

static void test_different_timing(void)
{
    ws2812_timing_t timing_72mhz = {
        .period_ticks  = 89U,
        .t0h_ticks     = 24U,
        .t1h_ticks     = 47U,
        .reset_periods = 80U,
    };
    ws2812_color_t c = {0xFF, 0, 0};
    uint16_t slots[24];

    printf("test_different_timing\n");
    ws2812_encode_grb(&c, &timing_72mhz, slots);

    /* G=0 → all T0H=24 */
    for (int i = 0; i < 8; i++) {
        ASSERT(slots[i] == 24U, "72MHz T0H");
    }
    /* R=0xFF → all T1H=47 */
    for (int i = 8; i < 16; i++) {
        ASSERT(slots[i] == 47U, "72MHz T1H");
    }
}

static void test_large_led_count(void)
{
    ws2812_color_t colors[30];

    printf("test_large_led_count\n");
    for (int i = 0; i < 30; i++) {
        colors[i].r = (uint8_t)(i * 8);
        colors[i].g = (uint8_t)(255 - i * 8);
        colors[i].b = 128;
    }

    mock_reset();
    ws2812_err_t err = ws2812_send(&mock_hal, &timing_48mhz, colors, 30);
    ASSERT(err == WS2812_OK, "send 30 leds ok");

    uint16_t vals[1024];
    int n = extract_data_compares(vals, 1024);
    ASSERT(n == 30 * 24 + 1, "721 compares for 30 LEDs");
}

static void test_all_color_corners(void)
{
    uint8_t vals[] = {0, 255};
    printf("test_all_color_corners\n");

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
                    ASSERT(slots[b] == expect_g, "G byte corner");
                for (int b = 8; b < 16; b++)
                    ASSERT(slots[b] == expect_r, "R byte corner");
                for (int b = 16; b < 24; b++)
                    ASSERT(slots[b] == expect_b, "B byte corner");
            }
        }
    }
}

static void test_mid_gray_encoding(void)
{
    ws2812_color_t c = {128, 128, 128};
    uint16_t slots[24];
    uint16_t t0 = timing_48mhz.t0h_ticks;
    uint16_t t1 = timing_48mhz.t1h_ticks;

    printf("test_mid_gray_encoding\n");
    ws2812_encode_grb(&c, &timing_48mhz, slots);

    /* 128 = 0x80 = 10000000, all three channels identical */
    uint16_t expected_byte[8] = { t1, t0, t0, t0, t0, t0, t0, t0 };
    for (int ch = 0; ch < 3; ch++) {
        for (int b = 0; b < 8; b++) {
            ASSERT(slots[ch * 8 + b] == expected_byte[b], "mid-gray bit");
        }
    }
}

static void test_data_inside_critical_section(void)
{
    ws2812_color_t c = {0xAA, 0x55, 0xCC};

    printf("test_data_inside_critical_section\n");
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
    ASSERT(data_outside_crit == 0, "no data bits outside critical section");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_null_params();
    test_single_led_black();
    test_single_led_white();
    test_grb_order();
    test_bit_encoding_pattern();
    test_multi_led();
    test_reset_latch();
    test_critical_section();
    test_scale5to8();
    test_different_timing();
    test_large_led_count();
    test_all_color_corners();
    test_mid_gray_encoding();
    test_data_inside_critical_section();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
