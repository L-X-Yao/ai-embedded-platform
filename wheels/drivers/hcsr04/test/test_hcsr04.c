// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hcsr04.h"
#include <string.h>

/* ---------- mock HAL state ---------- */

static bool     mock_trigger_pin;
static bool     mock_echo_pin;
static uint32_t mock_time_us;
static uint32_t mock_delay_total;

/* Echo simulation: after trigger pulse, echo goes HIGH at echo_start_time
 * and LOW at echo_end_time. */
static uint32_t mock_echo_start_time;
static uint32_t mock_echo_end_time;
static bool     mock_echo_never_high;   /* simulate no echo at all */
static bool     mock_echo_stuck_high;   /* simulate echo never goes LOW */

static void mock_reset(void)
{
    mock_trigger_pin     = false;
    mock_echo_pin        = false;
    mock_time_us         = 0;
    mock_delay_total     = 0;
    mock_echo_start_time = 0;
    mock_echo_end_time   = 0;
    mock_echo_never_high = false;
    mock_echo_stuck_high = false;
}

static void mock_trigger_high(void *ctx)
{
    (void)ctx;
    mock_trigger_pin = true;
}

static void mock_trigger_low(void *ctx)
{
    (void)ctx;
    mock_trigger_pin = false;
}

static bool mock_echo_read(void *ctx)
{
    (void)ctx;
    if (mock_echo_never_high)
    {
        return false;
    }
    if (mock_echo_stuck_high && mock_time_us >= mock_echo_start_time)
    {
        return true;
    }
    if (mock_time_us >= mock_echo_start_time && mock_time_us < mock_echo_end_time)
    {
        return true;
    }
    return false;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_delay_total += us;
    mock_time_us += us;
}

static uint32_t mock_get_time_us(void *ctx)
{
    (void)ctx;
    /* Advance time by 1us on each call to simulate polling */
    mock_time_us += 1;
    return mock_time_us;
}

static const hcsr04_hal_t mock_hal = {
    .ctx          = NULL,
    .trigger_high = mock_trigger_high,
    .trigger_low  = mock_trigger_low,
    .echo_read    = mock_echo_read,
    .delay_us     = mock_delay_us,
    .get_time_us  = mock_get_time_us,
};

/* ---------- helpers ---------- */

static hcsr04_t dev;

static void setup(void)
{
    mock_reset();
    (void)hcsr04_init(&dev, &mock_hal);
}

/* Set echo to appear after the trigger delay sequence completes.
 * The trigger sequence does: low + delay(2) + high + delay(10) + low
 * = 12us of delay, plus some get_time_us calls.
 * We set echo start slightly after trigger completes. */
static void setup_echo(uint32_t echo_duration_us)
{
    /* Echo starts at time ~30 (after trigger + some polling overhead) */
    mock_echo_start_time = 30;
    mock_echo_end_time   = 30 + echo_duration_us;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    hcsr04_t d;
    hcsr04_err_t e = hcsr04_init(&d, &mock_hal);
    ASSERT_EQ(e, HCSR04_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(hcsr04_init(NULL, &mock_hal), HCSR04_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    hcsr04_t d;
    ASSERT_EQ(hcsr04_init(&d, NULL), HCSR04_ERR_PARAM);
}

TEST(test_init_null_hal_fn)
{
    hcsr04_t d;
    hcsr04_hal_t bad_hal = mock_hal;

    bad_hal.trigger_high = NULL;
    ASSERT_EQ(hcsr04_init(&d, &bad_hal), HCSR04_ERR_PARAM);

    bad_hal = mock_hal;
    bad_hal.trigger_low = NULL;
    ASSERT_EQ(hcsr04_init(&d, &bad_hal), HCSR04_ERR_PARAM);

    bad_hal = mock_hal;
    bad_hal.echo_read = NULL;
    ASSERT_EQ(hcsr04_init(&d, &bad_hal), HCSR04_ERR_PARAM);

    bad_hal = mock_hal;
    bad_hal.delay_us = NULL;
    ASSERT_EQ(hcsr04_init(&d, &bad_hal), HCSR04_ERR_PARAM);

    bad_hal = mock_hal;
    bad_hal.get_time_us = NULL;
    ASSERT_EQ(hcsr04_init(&d, &bad_hal), HCSR04_ERR_PARAM);
}

TEST(test_read_cm_10cm)
{
    setup();
    /* 10cm -> echo ~583us (10 / 0.01715 = 583.1) */
    setup_echo(583);

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_OK);
    /* 583 * 0.01715 = 9.998 */
    ASSERT_NEAR(dist, 10.0f, 0.5f);
}

TEST(test_read_cm_100cm)
{
    setup();
    /* 100cm -> echo ~5831us */
    setup_echo(5831);

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_OK);
    ASSERT_NEAR(dist, 100.0f, 0.5f);
}

TEST(test_read_cm_max_range)
{
    setup();
    /* 400cm -> echo ~23324us (400 / 0.01715 = 23324).
     * Use 23300 to stay within range after polling overhead
     * (each get_time_us call advances mock clock by 1us). */
    setup_echo(23300);

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_OK);
    /* 23300 * 0.01715 = ~399.6cm, within 400cm range */
    ASSERT_NEAR(dist, 400.0f, 1.0f);
}

TEST(test_read_cm_min_range)
{
    setup();
    /* 2cm -> echo ~117us (2 / 0.01715 = 116.6) */
    setup_echo(117);

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_OK);
    ASSERT_NEAR(dist, 2.0f, 0.1f);
}

TEST(test_read_cm_below_min_range)
{
    setup();
    /* <2cm: echo too short, e.g. 50us -> 0.86cm */
    setup_echo(50);

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_ERR_RANGE);
}

TEST(test_read_cm_timeout_no_echo)
{
    setup();
    /* Echo never goes HIGH */
    mock_echo_never_high = true;

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_ERR_TIMEOUT);
}

TEST(test_read_cm_timeout_echo_stuck_high)
{
    setup();
    /* Echo goes HIGH but never goes LOW */
    mock_echo_stuck_high = true;
    mock_echo_start_time = 30;

    float dist;
    hcsr04_err_t e = hcsr04_read_cm(&dev, &dist);
    ASSERT_EQ(e, HCSR04_ERR_TIMEOUT);
}

TEST(test_read_us_normal)
{
    setup();
    setup_echo(1000);

    uint32_t us;
    hcsr04_err_t e = hcsr04_read_us(&dev, &us);
    ASSERT_EQ(e, HCSR04_OK);
    /* Should be close to 1000 (within polling granularity) */
    ASSERT_TRUE(us >= 990 && us <= 1010);
}

TEST(test_read_cm_null_dev)
{
    float dist;
    ASSERT_EQ(hcsr04_read_cm(NULL, &dist), HCSR04_ERR_PARAM);
}

TEST(test_read_cm_null_out)
{
    setup();
    ASSERT_EQ(hcsr04_read_cm(&dev, NULL), HCSR04_ERR_PARAM);
}

TEST(test_read_us_null_dev)
{
    uint32_t us;
    ASSERT_EQ(hcsr04_read_us(NULL, &us), HCSR04_ERR_PARAM);
}

TEST(test_read_us_null_out)
{
    setup();
    ASSERT_EQ(hcsr04_read_us(&dev, NULL), HCSR04_ERR_PARAM);
}

TEST_MAIN("hcsr04")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_null_hal_fn);
    RUN(test_read_cm_10cm);
    RUN(test_read_cm_100cm);
    RUN(test_read_cm_max_range);
    RUN(test_read_cm_min_range);
    RUN(test_read_cm_below_min_range);
    RUN(test_read_cm_timeout_no_echo);
    RUN(test_read_cm_timeout_echo_stuck_high);
    RUN(test_read_us_normal);
    RUN(test_read_cm_null_dev);
    RUN(test_read_cm_null_out);
    RUN(test_read_us_null_dev);
    RUN(test_read_us_null_out);
TEST_MAIN_END()
