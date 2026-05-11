// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "a4988.h"

/* ---------- mock HAL ---------- */

#define MAX_PIN_LOG 1024

typedef struct {
    uint8_t pin_id;
    bool    level;
} pin_event_t;

static pin_event_t pin_log[MAX_PIN_LOG];
static int         pin_log_idx;
static int         delay_us_total;
static int         delay_us_count;

static void mock_reset(void)
{
    memset(pin_log, 0, sizeof(pin_log));
    pin_log_idx = 0;
    delay_us_total = 0;
    delay_us_count = 0;
}

static void mock_write_pin(void *ctx, uint8_t pin_id, bool level)
{
    (void)ctx;
    if (pin_log_idx < MAX_PIN_LOG) {
        pin_log[pin_log_idx].pin_id = pin_id;
        pin_log[pin_log_idx].level = level;
        pin_log_idx++;
    }
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    delay_us_total += (int)us;
    delay_us_count++;
}

static const a4988_hal_t mock_hal = {
    .write_pin = mock_write_pin,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

static const a4988_pins_t test_pins = {
    .step   = 1,
    .dir    = 2,
    .enable = 3,
    .ms1    = 4,
    .ms2    = 5,
    .ms3    = 6,
};

/* Get the last pin state for a given pin_id */
static bool last_pin_state(uint8_t pin_id)
{
    for (int i = pin_log_idx - 1; i >= 0; i--) {
        if (pin_log[i].pin_id == pin_id) return pin_log[i].level;
    }
    return false;
}

/* Count how many times a pin was set to a level */
static int count_pin_events(uint8_t pin_id, bool level)
{
    int n = 0;
    for (int i = 0; i < pin_log_idx; i++) {
        if (pin_log[i].pin_id == pin_id && pin_log[i].level == level) n++;
    }
    return n;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));

    a4988_err_t e = a4988_init(&dev, &mock_hal, &test_pins);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_FALSE(dev.enabled);
    ASSERT_EQ(dev.direction, A4988_DIR_CW);
    ASSERT_EQ(dev.microstep, A4988_STEP_FULL);

    /* ENABLE should be HIGH (disabled) */
    ASSERT_TRUE(last_pin_state(3));
    /* STEP should be LOW */
    ASSERT_FALSE(last_pin_state(1));
    /* DIR should be LOW (CW) */
    ASSERT_FALSE(last_pin_state(2));
    /* MS1/MS2/MS3 should be LOW (full step) */
    ASSERT_FALSE(last_pin_state(4));
    ASSERT_FALSE(last_pin_state(5));
    ASSERT_FALSE(last_pin_state(6));
}

TEST(test_init_null_params)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));

    ASSERT_EQ(a4988_init(NULL, &mock_hal, &test_pins), A4988_ERR_PARAM);
    ASSERT_EQ(a4988_init(&dev, NULL, &test_pins), A4988_ERR_PARAM);
    ASSERT_EQ(a4988_init(&dev, &mock_hal, NULL), A4988_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));

    a4988_hal_t bad_hal = { .write_pin = NULL, .delay_us = mock_delay_us, .ctx = NULL };
    ASSERT_EQ(a4988_init(&dev, &bad_hal, &test_pins), A4988_ERR_PARAM);

    a4988_hal_t bad_hal2 = { .write_pin = mock_write_pin, .delay_us = NULL, .ctx = NULL };
    ASSERT_EQ(a4988_init(&dev, &bad_hal2, &test_pins), A4988_ERR_PARAM);
}

TEST(test_enable)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_enable(&dev, true);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(dev.enabled);
    /* ENABLE LOW = active */
    ASSERT_FALSE(last_pin_state(3));

    mock_reset();
    e = a4988_enable(&dev, false);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(dev.enabled);
    /* ENABLE HIGH = disabled */
    ASSERT_TRUE(last_pin_state(3));
}

TEST(test_enable_null)
{
    ASSERT_EQ(a4988_enable(NULL, true), A4988_ERR_PARAM);
}

TEST(test_set_direction)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_direction(&dev, A4988_DIR_CCW);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(dev.direction, A4988_DIR_CCW);
    ASSERT_TRUE(last_pin_state(2)); /* DIR HIGH = CCW */

    mock_reset();
    e = a4988_set_direction(&dev, A4988_DIR_CW);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(dev.direction, A4988_DIR_CW);
    ASSERT_FALSE(last_pin_state(2)); /* DIR LOW = CW */
}

TEST(test_set_direction_invalid)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    ASSERT_EQ(a4988_set_direction(&dev, (a4988_dir_t)5), A4988_ERR_PARAM);
}

TEST(test_set_direction_null)
{
    ASSERT_EQ(a4988_set_direction(NULL, A4988_DIR_CW), A4988_ERR_PARAM);
}

TEST(test_step)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_step(&dev);
    ASSERT_EQ(e, A4988_OK);

    /* Step pin should go HIGH then LOW */
    ASSERT_EQ(count_pin_events(1, true), 1);   /* STEP HIGH */
    ASSERT_EQ(count_pin_events(1, false), 1);  /* STEP LOW */
    /* Delay was called for pulse width */
    ASSERT_TRUE(delay_us_count >= 1);
    ASSERT_TRUE(delay_us_total >= 2);
}

TEST(test_step_null)
{
    ASSERT_EQ(a4988_step(NULL), A4988_ERR_PARAM);
}

TEST(test_step_n)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_step_n(&dev, 5, 100);
    ASSERT_EQ(e, A4988_OK);

    /* 5 step pulses -> 5 HIGH + 5 LOW */
    ASSERT_EQ(count_pin_events(1, true), 5);
    ASSERT_EQ(count_pin_events(1, false), 5);
    /* 5 pulse delays + 4 inter-step delays */
    ASSERT_EQ(delay_us_count, 9);
}

TEST(test_step_n_zero)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_step_n(&dev, 0, 100);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(pin_log_idx, 0); /* no pin events */
}

TEST(test_step_n_null)
{
    ASSERT_EQ(a4988_step_n(NULL, 5, 100), A4988_ERR_PARAM);
}

TEST(test_set_microstep_full)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_FULL);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_EQ(dev.microstep, A4988_STEP_FULL);
    ASSERT_FALSE(last_pin_state(4)); /* MS1=0 */
    ASSERT_FALSE(last_pin_state(5)); /* MS2=0 */
    ASSERT_FALSE(last_pin_state(6)); /* MS3=0 */
}

TEST(test_set_microstep_half)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_HALF);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(last_pin_state(4));  /* MS1=1 */
    ASSERT_FALSE(last_pin_state(5)); /* MS2=0 */
    ASSERT_FALSE(last_pin_state(6)); /* MS3=0 */
}

TEST(test_set_microstep_quarter)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_QUARTER);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_FALSE(last_pin_state(4)); /* MS1=0 */
    ASSERT_TRUE(last_pin_state(5));  /* MS2=1 */
    ASSERT_FALSE(last_pin_state(6)); /* MS3=0 */
}

TEST(test_set_microstep_eighth)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_EIGHTH);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(last_pin_state(4));  /* MS1=1 */
    ASSERT_TRUE(last_pin_state(5));  /* MS2=1 */
    ASSERT_FALSE(last_pin_state(6)); /* MS3=0 */
}

TEST(test_set_microstep_sixteenth)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_set_microstep(&dev, A4988_STEP_SIXTEENTH);
    ASSERT_EQ(e, A4988_OK);
    ASSERT_TRUE(last_pin_state(4)); /* MS1=1 */
    ASSERT_TRUE(last_pin_state(5)); /* MS2=1 */
    ASSERT_TRUE(last_pin_state(6)); /* MS3=1 */
}

TEST(test_set_microstep_invalid)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    ASSERT_EQ(a4988_set_microstep(&dev, (a4988_microstep_t)5), A4988_ERR_PARAM);
}

TEST(test_set_microstep_null)
{
    ASSERT_EQ(a4988_set_microstep(NULL, A4988_STEP_FULL), A4988_ERR_PARAM);
}

TEST(test_step_n_no_delay_between)
{
    mock_reset();
    a4988_t dev;
    memset(&dev, 0, sizeof(dev));
    a4988_init(&dev, &mock_hal, &test_pins);

    mock_reset();
    a4988_err_t e = a4988_step_n(&dev, 3, 0);
    ASSERT_EQ(e, A4988_OK);

    /* 3 step pulses, each with 1 delay for pulse width, no inter-step delays */
    ASSERT_EQ(count_pin_events(1, true), 3);
    ASSERT_EQ(delay_us_count, 3);
}

TEST_MAIN("a4988")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_init_null_callbacks);
    RUN(test_enable);
    RUN(test_enable_null);
    RUN(test_set_direction);
    RUN(test_set_direction_invalid);
    RUN(test_set_direction_null);
    RUN(test_step);
    RUN(test_step_null);
    RUN(test_step_n);
    RUN(test_step_n_zero);
    RUN(test_step_n_null);
    RUN(test_step_n_no_delay_between);
    RUN(test_set_microstep_full);
    RUN(test_set_microstep_half);
    RUN(test_set_microstep_quarter);
    RUN(test_set_microstep_eighth);
    RUN(test_set_microstep_sixteenth);
    RUN(test_set_microstep_invalid);
    RUN(test_set_microstep_null);
TEST_MAIN_END()