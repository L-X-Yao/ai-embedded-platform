/*
 * prop_tm1637.c -- property-based tests for tm1637
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_tm1637.c ../tm1637.c -o prop_tm1637
 */

#include "test_harness.h"
#include "property_test.h"
#include <string.h>
#include "../tm1637.h"

/* --- mock GPIO --- */

static bool mock_dio_state;

static void mock_clk_low(void *ctx)  { (void)ctx; }
static void mock_clk_high(void *ctx) { (void)ctx; }
static void mock_dio_low(void *ctx)  { (void)ctx; }
static void mock_dio_high(void *ctx) { (void)ctx; }
static bool mock_dio_read(void *ctx) { (void)ctx; return mock_dio_state; }
static void mock_delay_us(void *ctx, uint16_t us) { (void)ctx; (void)us; }

static const tm1637_gpio_t mock_gpio = {
    .clk_low  = mock_clk_low,
    .clk_high = mock_clk_high,
    .dio_low  = mock_dio_low,
    .dio_high = mock_dio_high,
    .dio_read = mock_dio_read,
    .delay_us = mock_delay_us,
    .ctx      = NULL,
};

static tm1637_t g_dev;

static void setup(void)
{
    mock_dio_state = false; /* ACK by default */
    tm1637_init(&g_dev, &mock_gpio);
}

/* --- init_no_crash: init never crashes --- */
PROPERTY(init_no_crash, 5000) {
    mock_dio_state = (ARBI_UINT8(seed) & 1) ? true : false;
    tm1637_t dev;
    /* May return ACK error but must not crash */
    (void)tm1637_init(&dev, &mock_gpio);
    PROP_ASSERT(1);
}

/* --- encode_digit_valid_range: digits 0-15 produce non-zero patterns --- */
PROPERTY(encode_digit_valid, 10000) {
    uint8_t d = (uint8_t)ARBI_RANGE(seed, 0, 15);
    uint8_t pat = tm1637_encode_digit(d);
    PROP_ASSERT(pat != 0x00);
}

/* --- encode_digit_out_of_range: digits >= 16 return 0 --- */
PROPERTY(encode_digit_oob_zero, 10000) {
    uint8_t d = (uint8_t)ARBI_RANGE(seed, 16, 255);
    uint8_t pat = tm1637_encode_digit(d);
    PROP_ASSERT_EQ(pat, 0x00);
}

/* --- display_number_no_crash: any int16 value handled without crash --- */
PROPERTY(display_number_no_crash, 10000) {
    setup();
    int16_t num = (int16_t)ARBI_UINT16(seed);
    bool leading = (ARBI_UINT8(seed) & 1) ? true : false;
    tm1637_err_t err = tm1637_display_number(&g_dev, num, leading);
    PROP_ASSERT(err == TM1637_OK);
}

/* --- brightness_valid_levels: levels 0-7 all accepted --- */
PROPERTY(brightness_valid, 10000) {
    setup();
    uint8_t level = (uint8_t)ARBI_RANGE(seed, 0, 7);
    bool on = (ARBI_UINT8(seed) & 1) ? true : false;
    tm1637_err_t err = tm1637_set_brightness(&g_dev, level, on);
    PROP_ASSERT(err == TM1637_OK);

    if (on) {
        PROP_ASSERT(g_dev.brightness == (uint8_t)(0x88 | level));
    } else {
        PROP_ASSERT(g_dev.brightness == 0x80);
    }
}

TEST_MAIN("tm1637 properties") {
    RUN(init_no_crash);
    RUN(encode_digit_valid);
    RUN(encode_digit_oob_zero);
    RUN(display_number_no_crash);
    RUN(brightness_valid);
}
TEST_MAIN_END()
