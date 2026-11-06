// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "../a4988.h"
#include <string.h>

/* ---------- mock GPIO ---------- */

#define MAX_PINS 16

static bool mock_pin_state[MAX_PINS];
static int  mock_pin_write_count;
static int  mock_delay_us_count;
static uint32_t mock_delay_us_total;

/* Track pin transitions for step verification */
typedef struct {
    uint8_t pin_id;
    bool    level;
} pin_event_t;

#define MAX_EVENTS 512
static pin_event_t mock_events[MAX_EVENTS];
static int         mock_event_count;

static void mock_reset_all(void)
{
    memset(mock_pin_state, 0, sizeof(mock_pin_state));
    mock_pin_write_count = 0;
    mock_delay_us_count = 0;
    mock_delay_us_total = 0;
    mock_event_count = 0;
    memset(mock_events, 0, sizeof(mock_events));
}

static void mock_write_pin(void *ctx, uint8_t pin_id, bool level)
{
    (void)ctx;
    if (pin_id < MAX_PINS) {
        mock_pin_state[pin_id] = level;
    }
    mock_pin_write_count++;

    if (mock_event_count < MAX_EVENTS) {
        mock_events[mock_event_count].pin_id = pin_id;
        mock_events[mock_event_count].level  = level;
        mock_event_count++;
    }
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_delay_us_count++;
    mock_delay_us_total += us;
}

static const a4988_hal_t mock_hal = {
    .write_pin = mock_write_pin,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

/* Pin assignments */
enum {
    PIN_STEP   = 0,
    PIN_DIR    = 1,
    PIN_ENABLE = 2,
    PIN_MS1    = 3,
    PIN_MS2    = 4,
    PIN_MS3    = 5,
};

static const a4988_cfg_t test_cfg = {
    .step   = PIN_STEP,
    .dir    = PIN_DIR,
    .enable = PIN_ENABLE,
    .ms1    = PIN_MS1,
    .ms2    = PIN_MS2,
    .ms3    = PIN_MS3,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));

    a4988_err_t e = a4988_init(&dev, &mock_hal, &test_cfg);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_FALSE(dev.enabled);
    ASSERT_EQ(dev.direction, A4988_DIR_CW);
    ASSERT_EQ(dev.microstep, A4988_STEP_FULL);

    /* ENABLE should be HIGH (disabled, active-LOW) */
    ASSERT_TRUE(mock_pin_state[PIN_ENABLE]);

    /* DIR LOW (CW) */
    ASSERT_FALSE(mock_pin_state[PIN_DIR]);

    /* STEP LOW */
    ASSERT_FALSE(mock_pin_state[PIN_STEP]);

    /* All MS pins LOW (full-step) */
    ASSERT_FALSE(mock_pin_state[PIN_MS1]);
    ASSERT_FALSE(mock_pin_state[PIN_MS2]);
    ASSERT_FALSE(mock_pin_state[PIN_MS3]);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(a4988_init(NULL, &mock_hal, &test_cfg), A4988_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(a4988_init(&dev, NULL, &test_cfg), A4988_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(a4988_init(&dev, &mock_hal, NULL), A4988_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));

    a4988_hal_t bad = {
        .write_pin = NULL,
        .delay_us  = mock_delay_us,
        .ctx       = NULL,
    };
    ASSERT_EQ(a4988_init(&dev, &bad, &test_cfg), A4988_ERR_PARAM);
}

TEST(test_enable)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    /* Enable: ENABLE pin goes LOW (active-LOW) */
    a4988_err_t e = a4988_enable(&dev, true);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(mock_pin_state[PIN_ENABLE]); /* LOW = enabled */
    ASSERT_TRUE(dev.enabled);
}

TEST(test_disable)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_enable(&dev, true);

    a4988_err_t e = a4988_enable(&dev, false);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(mock_pin_state[PIN_ENABLE]); /* HIGH = disabled */
    ASSERT_FALSE(dev.enabled);
}

TEST(test_set_direction_cw)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_direction(&dev, A4988_DIR_CW);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(mock_pin_state[PIN_DIR]);
    ASSERT_EQ(dev.direction, A4988_DIR_CW);
}

TEST(test_set_direction_ccw)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_direction(&dev, A4988_DIR_CCW);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(mock_pin_state[PIN_DIR]);
    ASSERT_EQ(dev.direction, A4988_DIR_CCW);
}

TEST(test_set_direction_invalid)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    ASSERT_EQ(a4988_set_direction(&dev, (a4988_dir_t)5), A4988_ERR_PARAM);
}

TEST(test_single_step)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);
    mock_event_count = 0;
    mock_delay_us_count = 0;

    a4988_err_t e = a4988_step(&dev);
    ASSERT_EQ(e, A4988_OK);

    /* Should see: STEP HIGH, delay, STEP LOW */
    ASSERT_TRUE(mock_event_count >= 2);
    ASSERT_EQ(mock_events[0].pin_id, PIN_STEP);
    ASSERT_TRUE(mock_events[0].level);           /* HIGH */
    ASSERT_EQ(mock_events[1].pin_id, PIN_STEP);
    ASSERT_FALSE(mock_events[1].level);           /* LOW */

    /* Delay should be 2us */
    ASSERT_TRUE(mock_delay_us_count >= 1);
    ASSERT_EQ(mock_delay_us_total, 2);

    /* STEP pin ends LOW */
    ASSERT_FALSE(mock_pin_state[PIN_STEP]);
}

TEST(test_step_n)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);
    mock_event_count = 0;
    mock_delay_us_count = 0;
    mock_delay_us_total = 0;

    a4988_err_t e = a4988_step_n(&dev, 5, 100);
    ASSERT_EQ(e, A4988_OK);

    /* 5 steps: each has HIGH + LOW = 10 pin events */
    ASSERT_EQ(mock_event_count, 10);

    /* Delays: 5 step pulses (2us each) + 4 inter-step delays (100us each) */
    /* Total: 5*2 + 4*100 = 410 */
    ASSERT_EQ(mock_delay_us_total, 410);
}

TEST(test_step_n_zero)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);
    mock_event_count = 0;

    a4988_err_t e = a4988_step_n(&dev, 0, 100);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(mock_event_count, 0); /* no steps taken */
}

TEST(test_step_n_one)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);
    mock_event_count = 0;
    mock_delay_us_total = 0;

    a4988_err_t e = a4988_step_n(&dev, 1, 100);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(mock_event_count, 2); /* one step: HIGH+LOW */
    /* Only pulse delay, no inter-step delay for single step */
    ASSERT_EQ(mock_delay_us_total, 2);
}

/* STEP LOW time before each rising edge, from the delays the HAL saw.
 * A4988 datasheet Figure 1: tB, STEP minimum LOW pulse width, is 1 us. */
static bool     gap_level;
static int      gap_pulses;
static uint32_t gap_low_us;
static uint32_t gap_min_low_us;

static void gap_write_pin(void *ctx, uint8_t pin_id, bool level)
{
    (void)ctx;
    if (pin_id != PIN_STEP) return;
    if (level && !gap_level) {
        if (gap_pulses > 0 && gap_low_us < gap_min_low_us) {
            gap_min_low_us = gap_low_us;
        }
        gap_pulses++;
    }
    if (!level) gap_low_us = 0;
    gap_level = level;
}

static void gap_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    if (!gap_level) gap_low_us += us;
}

static const a4988_hal_t gap_hal = {
    .write_pin = gap_write_pin,
    .delay_us  = gap_delay_us,
    .ctx       = NULL,
};

static void gap_reset(void)
{
    gap_level = false;
    gap_pulses = 0;
    gap_low_us = 0;
    gap_min_low_us = UINT32_MAX;
}

TEST(test_step_n_zero_delay_keeps_step_low_time)
{
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &gap_hal, &test_cfg);
    gap_reset();

    ASSERT_EQ(a4988_step_n(&dev, 5, 0), A4988_OK);
    ASSERT_EQ(gap_pulses, 5);
    ASSERT_TRUE(gap_min_low_us >= 1);
}

TEST(test_step_n_longer_delay_is_not_stretched)
{
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &gap_hal, &test_cfg);
    gap_reset();

    ASSERT_EQ(a4988_step_n(&dev, 5, 7), A4988_OK);
    ASSERT_EQ(gap_min_low_us, 7);
}

TEST(test_microstep_full)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_FULL);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(mock_pin_state[PIN_MS1]);
    ASSERT_FALSE(mock_pin_state[PIN_MS2]);
    ASSERT_FALSE(mock_pin_state[PIN_MS3]);
    ASSERT_EQ(dev.microstep, A4988_STEP_FULL);
}

TEST(test_microstep_half)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_HALF);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(mock_pin_state[PIN_MS1]);
    ASSERT_FALSE(mock_pin_state[PIN_MS2]);
    ASSERT_FALSE(mock_pin_state[PIN_MS3]);
    ASSERT_EQ(dev.microstep, A4988_STEP_HALF);
}

TEST(test_microstep_quarter)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_QUARTER);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(mock_pin_state[PIN_MS1]);
    ASSERT_TRUE(mock_pin_state[PIN_MS2]);
    ASSERT_FALSE(mock_pin_state[PIN_MS3]);
}

TEST(test_microstep_eighth)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_EIGHTH);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(mock_pin_state[PIN_MS1]);
    ASSERT_TRUE(mock_pin_state[PIN_MS2]);
    ASSERT_FALSE(mock_pin_state[PIN_MS3]);
}

TEST(test_microstep_sixteenth)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_SIXTEENTH);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(mock_pin_state[PIN_MS1]);
    ASSERT_TRUE(mock_pin_state[PIN_MS2]);
    ASSERT_TRUE(mock_pin_state[PIN_MS3]);
}

TEST(test_microstep_invalid)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);

    ASSERT_EQ(a4988_set_microstep(&dev, (a4988_microstep_t)10), A4988_ERR_PARAM);
}

TEST(test_enable_null)
{
    mock_reset_all();
    ASSERT_EQ(a4988_enable(NULL, true), A4988_ERR_PARAM);
}

TEST(test_set_direction_null)
{
    mock_reset_all();
    ASSERT_EQ(a4988_set_direction(NULL, A4988_DIR_CW), A4988_ERR_PARAM);
}

TEST(test_step_null)
{
    mock_reset_all();
    ASSERT_EQ(a4988_step(NULL), A4988_ERR_PARAM);
}

TEST(test_step_n_null)
{
    mock_reset_all();
    ASSERT_EQ(a4988_step_n(NULL, 5, 100), A4988_ERR_PARAM);
}

TEST(test_set_microstep_null)
{
    mock_reset_all();
    ASSERT_EQ(a4988_set_microstep(NULL, A4988_STEP_FULL), A4988_ERR_PARAM);
}

TEST(test_step_pulse_timing)
{
    mock_reset_all();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_cfg);
    mock_delay_us_total = 0;

    /* Each step has a 2us pulse */
    a4988_step(&dev);
    ASSERT_TRUE(mock_delay_us_total >= 2);
}

TEST_MAIN("a4988")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_null_cfg);
    RUN(test_init_null_callbacks);
    RUN(test_enable);
    RUN(test_disable);
    RUN(test_set_direction_cw);
    RUN(test_set_direction_ccw);
    RUN(test_set_direction_invalid);
    RUN(test_single_step);
    RUN(test_step_n);
    RUN(test_step_n_zero);
    RUN(test_step_n_one);
    RUN(test_step_n_zero_delay_keeps_step_low_time);
    RUN(test_step_n_longer_delay_is_not_stretched);
    RUN(test_step_pulse_timing);
    RUN(test_microstep_full);
    RUN(test_microstep_half);
    RUN(test_microstep_quarter);
    RUN(test_microstep_eighth);
    RUN(test_microstep_sixteenth);
    RUN(test_microstep_invalid);
    RUN(test_enable_null);
    RUN(test_set_direction_null);
    RUN(test_step_null);
    RUN(test_step_n_null);
    RUN(test_set_microstep_null);
TEST_MAIN_END()
