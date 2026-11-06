// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "tm1637.h"
#include <string.h>

/* ---------- mock GPIO with protocol trace ---------- */

#define TRACE_MAX 512

typedef enum {
    OP_CLK_LOW,
    OP_CLK_HIGH,
    OP_DIO_LOW,
    OP_DIO_HIGH,
    OP_DIO_READ,
    OP_DELAY,
} mock_op_t;

static mock_op_t  trace_ops[TRACE_MAX];
static int        trace_len;
static bool       mock_dio_state;  /* what dio_read returns */
static int        mock_ack_nack_at;  /* which byte (0-based) to NACK, -1 = all ACK */
static int        mock_byte_count;   /* counts completed bytes */

static void mock_reset(void)
{
    trace_len = 0;
    mock_dio_state = false;   /* ACK by default (DIO low = ACK) */
    mock_ack_nack_at = -1;
    mock_byte_count = 0;
}

static void trace_push(mock_op_t op)
{
    if (trace_len < TRACE_MAX) {
        trace_ops[trace_len++] = op;
    }
}

static void mock_clk_low(void *ctx)  { (void)ctx; trace_push(OP_CLK_LOW); }
static void mock_clk_high(void *ctx) { (void)ctx; trace_push(OP_CLK_HIGH); }
static void mock_dio_low(void *ctx)  { (void)ctx; trace_push(OP_DIO_LOW); }
static void mock_dio_high(void *ctx) { (void)ctx; trace_push(OP_DIO_HIGH); }

static bool mock_dio_read(void *ctx)
{
    (void)ctx;
    trace_push(OP_DIO_READ);

    /* Count DIO reads to track byte ACKs.
     * Each byte = 8 data clocks + 1 ACK clock.
     * The dio_read during ACK is what we control. */
    mock_byte_count++;

    if (mock_ack_nack_at >= 0 && mock_byte_count > mock_ack_nack_at) {
        return true;  /* NACK: DIO stays high */
    }
    return mock_dio_state;  /* default: false = ACK */
}

static void mock_delay_us(void *ctx, uint16_t us)
{
    (void)ctx; (void)us;
    trace_push(OP_DELAY);
}

static const tm1637_gpio_t mock_gpio = {
    .clk_low  = mock_clk_low,
    .clk_high = mock_clk_high,
    .dio_low  = mock_dio_low,
    .dio_high = mock_dio_high,
    .dio_read = mock_dio_read,
    .delay_us = mock_delay_us,
    .ctx      = NULL,
};

/* ---------- helper: init a device for tests ---------- */

static tm1637_t dev;

static void setup(void)
{
    mock_reset();
    tm1637_init(&dev, &mock_gpio);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    tm1637_t d;
    tm1637_err_t e = tm1637_init(&d, &mock_gpio);
    ASSERT_EQ(e, TM1637_OK);
    /* Default brightness: on + level 2 */
    ASSERT_EQ_HEX(d.brightness, 0x8A);
    /* Shadow buffer should be all zeros */
    for (int i = 0; i < TM1637_DIGITS; i++) {
        ASSERT_EQ(d.segments[i], 0);
    }
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(tm1637_init(NULL, &mock_gpio), TM1637_ERR_PARAM);
}

TEST(test_init_null_gpio)
{
    tm1637_t d;
    ASSERT_EQ(tm1637_init(&d, NULL), TM1637_ERR_PARAM);
}

TEST(test_init_null_callback)
{
    tm1637_gpio_t bad = mock_gpio;

    bad.clk_low = NULL;
    tm1637_t d;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);

    bad = mock_gpio;
    bad.clk_high = NULL;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);

    bad = mock_gpio;
    bad.dio_low = NULL;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);

    bad = mock_gpio;
    bad.dio_high = NULL;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);

    bad = mock_gpio;
    bad.dio_read = NULL;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);

    bad = mock_gpio;
    bad.delay_us = NULL;
    ASSERT_EQ(tm1637_init(&d, &bad), TM1637_ERR_PARAM);
}

TEST(test_init_generates_bus_activity)
{
    mock_reset();
    tm1637_t d;
    tm1637_init(&d, &mock_gpio);
    /* Init should produce bus activity (start/stop/data) */
    ASSERT_TRUE(trace_len > 0);
}

/* ---------- brightness tests ---------- */

TEST(test_brightness_ok)
{
    setup();
    ASSERT_EQ(tm1637_set_brightness(&dev, 0, true), TM1637_OK);
    ASSERT_EQ_HEX(dev.brightness, 0x88);

    ASSERT_EQ(tm1637_set_brightness(&dev, 7, true), TM1637_OK);
    ASSERT_EQ_HEX(dev.brightness, 0x8F);

    ASSERT_EQ(tm1637_set_brightness(&dev, 3, true), TM1637_OK);
    ASSERT_EQ_HEX(dev.brightness, 0x8B);
}

TEST(test_brightness_off)
{
    setup();
    ASSERT_EQ(tm1637_set_brightness(&dev, 5, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.brightness, 0x80);
}

TEST(test_brightness_level_too_high)
{
    setup();
    ASSERT_EQ(tm1637_set_brightness(&dev, 8, true), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_set_brightness(&dev, 255, true), TM1637_ERR_PARAM);
}

TEST(test_brightness_null)
{
    ASSERT_EQ(tm1637_set_brightness(NULL, 3, true), TM1637_ERR_PARAM);

    tm1637_t d;
    memset(&d, 0, sizeof(d));
    ASSERT_EQ(tm1637_set_brightness(&d, 3, true), TM1637_ERR_PARAM);
}

/* ---------- encode digit tests ---------- */

TEST(test_encode_digit_0_to_9)
{
    ASSERT_EQ_HEX(tm1637_encode_digit(0), 0x3F);
    ASSERT_EQ_HEX(tm1637_encode_digit(1), 0x06);
    ASSERT_EQ_HEX(tm1637_encode_digit(2), 0x5B);
    ASSERT_EQ_HEX(tm1637_encode_digit(3), 0x4F);
    ASSERT_EQ_HEX(tm1637_encode_digit(4), 0x66);
    ASSERT_EQ_HEX(tm1637_encode_digit(5), 0x6D);
    ASSERT_EQ_HEX(tm1637_encode_digit(6), 0x7D);
    ASSERT_EQ_HEX(tm1637_encode_digit(7), 0x07);
    ASSERT_EQ_HEX(tm1637_encode_digit(8), 0x7F);
    ASSERT_EQ_HEX(tm1637_encode_digit(9), 0x6F);
}

TEST(test_encode_digit_hex)
{
    ASSERT_EQ_HEX(tm1637_encode_digit(0xA), 0x77);
    ASSERT_EQ_HEX(tm1637_encode_digit(0xB), 0x7C);
    ASSERT_EQ_HEX(tm1637_encode_digit(0xC), 0x39);
    ASSERT_EQ_HEX(tm1637_encode_digit(0xD), 0x5E);
    ASSERT_EQ_HEX(tm1637_encode_digit(0xE), 0x79);
    ASSERT_EQ_HEX(tm1637_encode_digit(0xF), 0x71);
}

TEST(test_encode_digit_out_of_range)
{
    ASSERT_EQ(tm1637_encode_digit(16), 0x00);
    ASSERT_EQ(tm1637_encode_digit(255), 0x00);
}

/* ---------- display segments tests ---------- */

TEST(test_display_segments_ok)
{
    setup();
    uint8_t segs[4] = { 0x3F, 0x06, 0x5B, 0x4F };
    ASSERT_EQ(tm1637_display_segments(&dev, segs, 4), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x3F);
    ASSERT_EQ_HEX(dev.segments[1], 0x06);
    ASSERT_EQ_HEX(dev.segments[2], 0x5B);
    ASSERT_EQ_HEX(dev.segments[3], 0x4F);
}

TEST(test_display_segments_partial)
{
    setup();
    /* First set all digits */
    uint8_t segs4[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    tm1637_display_segments(&dev, segs4, 4);

    /* Update only first 2 */
    uint8_t segs2[2] = { 0x3F, 0x06 };
    ASSERT_EQ(tm1637_display_segments(&dev, segs2, 2), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x3F);
    ASSERT_EQ_HEX(dev.segments[1], 0x06);
    /* positions 2-3 unchanged in shadow */
    ASSERT_EQ_HEX(dev.segments[2], 0xFF);
    ASSERT_EQ_HEX(dev.segments[3], 0xFF);
}

TEST(test_display_segments_null)
{
    setup();
    ASSERT_EQ(tm1637_display_segments(NULL, (const uint8_t *)"", 1), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_display_segments(&dev, NULL, 1), TM1637_ERR_PARAM);
}

TEST(test_display_segments_zero_count)
{
    setup();
    uint8_t segs[1] = { 0x3F };
    ASSERT_EQ(tm1637_display_segments(&dev, segs, 0), TM1637_ERR_PARAM);
}

TEST(test_display_segments_count_too_large)
{
    setup();
    uint8_t segs[5] = { 0 };
    ASSERT_EQ(tm1637_display_segments(&dev, segs, 5), TM1637_ERR_PARAM);
}

/* ---------- display number tests ---------- */

TEST(test_display_number_zero)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 0, false), TM1637_OK);
    /* "   0" — three blanks + digit 0 */
    ASSERT_EQ_HEX(dev.segments[0], 0x00);
    ASSERT_EQ_HEX(dev.segments[1], 0x00);
    ASSERT_EQ_HEX(dev.segments[2], 0x00);
    ASSERT_EQ_HEX(dev.segments[3], 0x3F);
}

TEST(test_display_number_zero_leading)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 0, true), TM1637_OK);
    /* "0000" */
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ_HEX(dev.segments[i], 0x3F);
    }
}

TEST(test_display_number_1234)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 1234, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x06); /* 1 */
    ASSERT_EQ_HEX(dev.segments[1], 0x5B); /* 2 */
    ASSERT_EQ_HEX(dev.segments[2], 0x4F); /* 3 */
    ASSERT_EQ_HEX(dev.segments[3], 0x66); /* 4 */
}

TEST(test_display_number_42)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 42, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[1], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[2], 0x66); /* 4 */
    ASSERT_EQ_HEX(dev.segments[3], 0x5B); /* 2 */
}

TEST(test_display_number_42_leading_zeros)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 42, true), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x3F); /* 0 */
    ASSERT_EQ_HEX(dev.segments[1], 0x3F); /* 0 */
    ASSERT_EQ_HEX(dev.segments[2], 0x66); /* 4 */
    ASSERT_EQ_HEX(dev.segments[3], 0x5B); /* 2 */
}

TEST(test_display_number_negative)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, -5, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[1], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[2], 0x40); /* minus */
    ASSERT_EQ_HEX(dev.segments[3], 0x6D); /* 5 */
}

TEST(test_display_number_negative_123)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, -123, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x40); /* minus */
    ASSERT_EQ_HEX(dev.segments[1], 0x06); /* 1 */
    ASSERT_EQ_HEX(dev.segments[2], 0x5B); /* 2 */
    ASSERT_EQ_HEX(dev.segments[3], 0x4F); /* 3 */
}

TEST(test_display_number_9999)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 9999, false), TM1637_OK);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ_HEX(dev.segments[i], 0x6F); /* 9 */
    }
}

TEST(test_display_number_clamp_positive)
{
    setup();
    /* 10000 should be clamped to 9999 */
    ASSERT_EQ(tm1637_display_number(&dev, 10000, false), TM1637_OK);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ_HEX(dev.segments[i], 0x6F);
    }
}

TEST(test_display_number_clamp_negative)
{
    setup();
    /* -1000 should be clamped to -999 */
    ASSERT_EQ(tm1637_display_number(&dev, -1000, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x40); /* minus */
    ASSERT_EQ_HEX(dev.segments[1], 0x6F); /* 9 */
    ASSERT_EQ_HEX(dev.segments[2], 0x6F); /* 9 */
    ASSERT_EQ_HEX(dev.segments[3], 0x6F); /* 9 */
}

TEST(test_display_number_null)
{
    ASSERT_EQ(tm1637_display_number(NULL, 42, false), TM1637_ERR_PARAM);
}

/* ---------- display digit tests ---------- */

TEST(test_display_digit_ok)
{
    setup();
    ASSERT_EQ(tm1637_display_digit(&dev, 0, 5), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x6D);

    ASSERT_EQ(tm1637_display_digit(&dev, 3, 0xF), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[3], 0x71);
}

TEST(test_display_digit_preserves_colon)
{
    setup();
    /* Set colon first */
    dev.segments[1] = 0x86;  /* digit 1 + colon */
    ASSERT_EQ(tm1637_display_digit(&dev, 1, 3), TM1637_OK);
    /* Should have digit 3 (0x4F) + colon bit (0x80) */
    ASSERT_EQ_HEX(dev.segments[1], 0xCF);
}

TEST(test_display_digit_position_oob)
{
    setup();
    ASSERT_EQ(tm1637_display_digit(&dev, 4, 0), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_display_digit(&dev, 255, 0), TM1637_ERR_PARAM);
}

TEST(test_display_digit_value_oob)
{
    setup();
    ASSERT_EQ(tm1637_display_digit(&dev, 0, 16), TM1637_ERR_PARAM);
}

TEST(test_display_digit_null)
{
    ASSERT_EQ(tm1637_display_digit(NULL, 0, 5), TM1637_ERR_PARAM);
}

/* ---------- colon tests ---------- */

TEST(test_colon_on)
{
    setup();
    dev.segments[1] = 0x06;  /* digit 1 */
    ASSERT_EQ(tm1637_set_colon(&dev, true), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[1], 0x86);
}

TEST(test_colon_off)
{
    setup();
    dev.segments[1] = 0x86;  /* digit 1 + colon */
    ASSERT_EQ(tm1637_set_colon(&dev, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[1], 0x06);
}

TEST(test_colon_null)
{
    ASSERT_EQ(tm1637_set_colon(NULL, true), TM1637_ERR_PARAM);
}

/* ---------- clear tests ---------- */

TEST(test_clear)
{
    setup();
    /* Fill with data */
    dev.segments[0] = 0xFF;
    dev.segments[1] = 0xFF;
    dev.segments[2] = 0xFF;
    dev.segments[3] = 0xFF;

    ASSERT_EQ(tm1637_clear(&dev), TM1637_OK);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(dev.segments[i], 0x00);
    }
}

TEST(test_clear_null)
{
    ASSERT_EQ(tm1637_clear(NULL), TM1637_ERR_PARAM);
}

/* ---------- ACK error tests ---------- */

TEST(test_init_ack_error)
{
    mock_reset();
    mock_ack_nack_at = 0;  /* NACK on very first byte */
    tm1637_t d;
    ASSERT_EQ(tm1637_init(&d, &mock_gpio), TM1637_ERR_ACK);
}

TEST(test_brightness_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_set_brightness(&dev, 3, true), TM1637_ERR_ACK);
}

TEST(test_display_segments_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    uint8_t segs[4] = { 0x3F, 0x06, 0x5B, 0x4F };
    ASSERT_EQ(tm1637_display_segments(&dev, segs, 4), TM1637_ERR_ACK);
}

TEST(test_display_number_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_display_number(&dev, 42, false), TM1637_ERR_ACK);
}

TEST(test_colon_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_set_colon(&dev, true), TM1637_ERR_ACK);
}

TEST(test_clear_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_clear(&dev), TM1637_ERR_ACK);
}

TEST(test_display_digit_ack_error)
{
    setup();
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_display_digit(&dev, 0, 5), TM1637_ERR_ACK);
}

/* ---------- protocol trace tests ---------- */

TEST(test_protocol_start_condition)
{
    /* Start condition: DIO high -> CLK high -> DIO low -> CLK low */
    setup();
    mock_reset();

    /* Clear triggers a flush which includes start conditions */
    tm1637_clear(&dev);

    /* Check that the trace starts with a start condition pattern */
    ASSERT_TRUE(trace_len > 8);
    /* First meaningful sequence: DIO_HIGH, CLK_HIGH, delay, DIO_LOW, delay, CLK_LOW */
    int found_start = 0;
    for (int i = 0; i < trace_len - 5; i++) {
        if (trace_ops[i] == OP_DIO_HIGH &&
            trace_ops[i+1] == OP_CLK_HIGH &&
            trace_ops[i+2] == OP_DELAY &&
            trace_ops[i+3] == OP_DIO_LOW &&
            trace_ops[i+4] == OP_DELAY &&
            trace_ops[i+5] == OP_CLK_LOW) {
            found_start = 1;
            break;
        }
    }
    ASSERT_TRUE(found_start);
}

TEST(test_protocol_stop_condition)
{
    /* Stop condition: CLK low -> DIO low -> CLK high -> DIO high */
    setup();
    mock_reset();
    tm1637_clear(&dev);

    int found_stop = 0;
    for (int i = 0; i < trace_len - 7; i++) {
        if (trace_ops[i] == OP_CLK_LOW &&
            trace_ops[i+1] == OP_DELAY &&
            trace_ops[i+2] == OP_DIO_LOW &&
            trace_ops[i+3] == OP_DELAY &&
            trace_ops[i+4] == OP_CLK_HIGH &&
            trace_ops[i+5] == OP_DELAY &&
            trace_ops[i+6] == OP_DIO_HIGH &&
            trace_ops[i+7] == OP_DELAY) {
            found_stop = 1;
            break;
        }
    }
    ASSERT_TRUE(found_stop);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(TM1637_OK, 0);
    ASSERT_EQ(TM1637_ERR_PARAM, -1);
    ASSERT_EQ(TM1637_ERR_ACK, -2);
}

/* ---------- edge cases ---------- */

TEST(test_display_number_single_digit)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 7, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[3], 0x07); /* 7 */
    ASSERT_EQ_HEX(dev.segments[0], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[1], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[2], 0x00); /* blank */
}

TEST(test_display_number_negative_1)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, -1, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[1], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[2], 0x40); /* minus */
    ASSERT_EQ_HEX(dev.segments[3], 0x06); /* 1 */
}

TEST(test_display_number_100)
{
    setup();
    ASSERT_EQ(tm1637_display_number(&dev, 100, false), TM1637_OK);
    ASSERT_EQ_HEX(dev.segments[0], 0x00); /* blank */
    ASSERT_EQ_HEX(dev.segments[1], 0x06); /* 1 */
    ASSERT_EQ_HEX(dev.segments[2], 0x3F); /* 0 */
    ASSERT_EQ_HEX(dev.segments[3], 0x3F); /* 0 */
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(tm1637_t *d)
{
    const uint8_t segments[1] = { 0x3F };
    ASSERT_EQ(tm1637_set_brightness(d, 2, true), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_display_segments(d, segments, 1), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_display_number(d, 1234, false), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_display_digit(d, 0, 1), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_set_colon(d, true), TM1637_ERR_PARAM);
    ASSERT_EQ(tm1637_clear(d), TM1637_ERR_PARAM);
}

TEST(test_device_refused_after_nack)
{
    tm1637_t d;
    mock_reset();
    mock_ack_nack_at = 0;
    ASSERT_EQ(tm1637_init(&d, &mock_gpio), TM1637_ERR_ACK);
    mock_reset();  /* the display answers again: only init can bring it up */
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const tm1637_gpio_t zeroed;
    tm1637_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tm1637_init(&d, NULL), TM1637_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tm1637_init(&d, &zeroed), TM1637_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("tm1637")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_gpio);
    RUN(test_init_null_callback);
    RUN(test_init_generates_bus_activity);
    RUN(test_brightness_ok);
    RUN(test_brightness_off);
    RUN(test_brightness_level_too_high);
    RUN(test_brightness_null);
    RUN(test_encode_digit_0_to_9);
    RUN(test_encode_digit_hex);
    RUN(test_encode_digit_out_of_range);
    RUN(test_display_segments_ok);
    RUN(test_display_segments_partial);
    RUN(test_display_segments_null);
    RUN(test_display_segments_zero_count);
    RUN(test_display_segments_count_too_large);
    RUN(test_display_number_zero);
    RUN(test_display_number_zero_leading);
    RUN(test_display_number_1234);
    RUN(test_display_number_42);
    RUN(test_display_number_42_leading_zeros);
    RUN(test_display_number_negative);
    RUN(test_display_number_negative_123);
    RUN(test_display_number_9999);
    RUN(test_display_number_clamp_positive);
    RUN(test_display_number_clamp_negative);
    RUN(test_display_number_null);
    RUN(test_display_digit_ok);
    RUN(test_display_digit_preserves_colon);
    RUN(test_display_digit_position_oob);
    RUN(test_display_digit_value_oob);
    RUN(test_display_digit_null);
    RUN(test_colon_on);
    RUN(test_colon_off);
    RUN(test_colon_null);
    RUN(test_clear);
    RUN(test_clear_null);
    RUN(test_init_ack_error);
    RUN(test_brightness_ack_error);
    RUN(test_display_segments_ack_error);
    RUN(test_display_number_ack_error);
    RUN(test_colon_ack_error);
    RUN(test_clear_ack_error);
    RUN(test_display_digit_ack_error);
    RUN(test_protocol_start_condition);
    RUN(test_protocol_stop_condition);
    RUN(test_error_codes);
    RUN(test_display_number_single_digit);
    RUN(test_display_number_negative_1);
    RUN(test_display_number_100);
    RUN(test_device_refused_after_nack);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
