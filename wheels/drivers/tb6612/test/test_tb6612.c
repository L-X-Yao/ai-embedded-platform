// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "tb6612.h"

/* ---------- mock HAL ---------- */

#define MAX_PIN_LOG 256

typedef struct {
    uint8_t pin_id;
    bool    level;
} pin_event_t;

typedef struct {
    uint8_t  channel;
    uint16_t duty;
} pwm_event_t;

static pin_event_t pin_log[MAX_PIN_LOG];
static int         pin_log_idx;
static pwm_event_t pwm_log[MAX_PIN_LOG];
static int         pwm_log_idx;

static void mock_reset(void)
{
    memset(pin_log, 0, sizeof(pin_log));
    pin_log_idx = 0;
    memset(pwm_log, 0, sizeof(pwm_log));
    pwm_log_idx = 0;
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

static void mock_set_pwm(void *ctx, uint8_t channel, uint16_t duty)
{
    (void)ctx;
    if (pwm_log_idx < MAX_PIN_LOG) {
        pwm_log[pwm_log_idx].channel = channel;
        pwm_log[pwm_log_idx].duty = duty;
        pwm_log_idx++;
    }
}

static const tb6612_hal_t mock_hal = {
    .write_pin    = mock_write_pin,
    .set_pwm_duty = mock_set_pwm,
    .ctx          = NULL,
};

static const tb6612_pins_t test_pins = {
    .ain1 = 1,
    .ain2 = 2,
    .bin1 = 3,
    .bin2 = 4,
    .stby = 5,
    .pwma = 6,
    .pwmb = 7,
};

/* Get the last pin state for a given pin_id */
static bool last_pin_state(uint8_t pin_id)
{
    for (int i = pin_log_idx - 1; i >= 0; i--) {
        if (pin_log[i].pin_id == pin_id) return pin_log[i].level;
    }
    return false;
}

/* Get the last PWM duty for a given channel */
static uint16_t last_pwm_duty(uint8_t channel)
{
    for (int i = pwm_log_idx - 1; i >= 0; i--) {
        if (pwm_log[i].channel == channel) return pwm_log[i].duty;
    }
    return 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));

    tb6612_err_t e = tb6612_init(&dev, &mock_hal, &test_pins, 1000);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_EQ(dev.max_duty, 1000);
    /* STBY should be LOW after init */
    ASSERT_FALSE(last_pin_state(5));
}

TEST(test_init_null_params)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));

    ASSERT_EQ(tb6612_init(NULL, &mock_hal, &test_pins, 1000), TB6612_ERR_PARAM);
    ASSERT_EQ(tb6612_init(&dev, NULL, &test_pins, 1000), TB6612_ERR_PARAM);
    ASSERT_EQ(tb6612_init(&dev, &mock_hal, NULL, 1000), TB6612_ERR_PARAM);
    ASSERT_EQ(tb6612_init(&dev, &mock_hal, &test_pins, 0), TB6612_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));

    tb6612_hal_t bad_hal = { .write_pin = NULL, .set_pwm_duty = mock_set_pwm, .ctx = NULL };
    ASSERT_EQ(tb6612_init(&dev, &bad_hal, &test_pins, 1000), TB6612_ERR_PARAM);

    tb6612_hal_t bad_hal2 = { .write_pin = mock_write_pin, .set_pwm_duty = NULL, .ctx = NULL };
    ASSERT_EQ(tb6612_init(&dev, &bad_hal2, &test_pins, 1000), TB6612_ERR_PARAM);
}

TEST(test_set_speed_forward_a)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset(); /* clear init events */
    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, 500);
    ASSERT_EQ(e, TB6612_OK);

    /* CW: AIN1=H, AIN2=L */
    ASSERT_TRUE(last_pin_state(1));   /* AIN1 = HIGH */
    ASSERT_FALSE(last_pin_state(2));  /* AIN2 = LOW */
    /* PWM duty = 500 * 1000 / 1000 = 500 */
    ASSERT_EQ(last_pwm_duty(6), 500);
}

TEST(test_set_speed_reverse_a)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, -750);
    ASSERT_EQ(e, TB6612_OK);

    /* CCW: AIN1=L, AIN2=H */
    ASSERT_FALSE(last_pin_state(1));
    ASSERT_TRUE(last_pin_state(2));
    ASSERT_EQ(last_pwm_duty(6), 750);
}

TEST(test_set_speed_stop)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, 0);
    ASSERT_EQ(e, TB6612_OK);

    /* Stop: AIN1=L, AIN2=L */
    ASSERT_FALSE(last_pin_state(1));
    ASSERT_FALSE(last_pin_state(2));
    ASSERT_EQ(last_pwm_duty(6), 0);
}

TEST(test_set_speed_motor_b)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_B, 1000);
    ASSERT_EQ(e, TB6612_OK);

    /* CW on B: BIN1=H, BIN2=L */
    ASSERT_TRUE(last_pin_state(3));   /* BIN1 = HIGH */
    ASSERT_FALSE(last_pin_state(4));  /* BIN2 = LOW */
    ASSERT_EQ(last_pwm_duty(7), 1000);
}

TEST(test_set_speed_clamp)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    /* Over max -> clamped to 1000 */
    tb6612_set_speed(&dev, TB6612_MOTOR_A, 2000);
    ASSERT_EQ(last_pwm_duty(6), 1000);

    mock_reset();
    /* Under min -> clamped to -1000 */
    tb6612_set_speed(&dev, TB6612_MOTOR_A, -2000);
    ASSERT_EQ(last_pwm_duty(6), 1000);
}

TEST(test_set_speed_invalid_channel)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    ASSERT_EQ(tb6612_set_speed(&dev, (tb6612_channel_t)2, 500), TB6612_ERR_PARAM);
}

TEST(test_brake)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    tb6612_err_t e = tb6612_brake(&dev, TB6612_MOTOR_A);
    ASSERT_EQ(e, TB6612_OK);

    /* Brake: IN1=H, IN2=H, PWM=max */
    ASSERT_TRUE(last_pin_state(1));
    ASSERT_TRUE(last_pin_state(2));
    ASSERT_EQ(last_pwm_duty(6), 1000);
}

TEST(test_brake_null)
{
    ASSERT_EQ(tb6612_brake(NULL, TB6612_MOTOR_A), TB6612_ERR_PARAM);
}

TEST(test_standby)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_pins, 1000);

    mock_reset();
    /* Enter standby: STBY LOW */
    tb6612_err_t e = tb6612_standby(&dev, true);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_FALSE(last_pin_state(5));

    mock_reset();
    /* Exit standby: STBY HIGH */
    e = tb6612_standby(&dev, false);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_TRUE(last_pin_state(5));
}

TEST(test_standby_null)
{
    ASSERT_EQ(tb6612_standby(NULL, true), TB6612_ERR_PARAM);
}

TEST(test_set_speed_null)
{
    ASSERT_EQ(tb6612_set_speed(NULL, TB6612_MOTOR_A, 500), TB6612_ERR_PARAM);
}

TEST(test_duty_scaling)
{
    mock_reset();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    /* Use max_duty=500 to test scaling */
    tb6612_init(&dev, &mock_hal, &test_pins, 500);

    mock_reset();
    tb6612_set_speed(&dev, TB6612_MOTOR_A, 500);
    /* duty = 500 * 500 / 1000 = 250 */
    ASSERT_EQ(last_pwm_duty(6), 250);
}

TEST_MAIN("tb6612")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_init_null_callbacks);
    RUN(test_set_speed_forward_a);
    RUN(test_set_speed_reverse_a);
    RUN(test_set_speed_stop);
    RUN(test_set_speed_motor_b);
    RUN(test_set_speed_clamp);
    RUN(test_set_speed_invalid_channel);
    RUN(test_set_speed_null);
    RUN(test_brake);
    RUN(test_brake_null);
    RUN(test_standby);
    RUN(test_standby_null);
    RUN(test_duty_scaling);
TEST_MAIN_END()