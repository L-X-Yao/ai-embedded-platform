// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "../tb6612.h"
#include <string.h>

/* ---------- mock GPIO + PWM ---------- */

#define MAX_PINS 16

static bool    mock_pin_state[MAX_PINS];
static int     mock_pin_write_count;
static uint16_t mock_pwm_duty[MAX_PINS];
static int     mock_pwm_write_count;

/* Track last operations per pin for precise verification */
typedef struct {
    uint8_t  pin_id;
    bool     level;
} pin_op_t;

static pin_op_t mock_pin_ops[64];
static int      mock_pin_op_count;

typedef struct {
    uint8_t  channel;
    uint16_t duty;
} pwm_op_t;

static pwm_op_t mock_pwm_ops[64];
static int      mock_pwm_op_count;

static void mock_reset_all(void)
{
    memset(mock_pin_state, 0, sizeof(mock_pin_state));
    mock_pin_write_count = 0;
    memset(mock_pwm_duty, 0, sizeof(mock_pwm_duty));
    mock_pwm_write_count = 0;
    mock_pin_op_count = 0;
    mock_pwm_op_count = 0;
    memset(mock_pin_ops, 0, sizeof(mock_pin_ops));
    memset(mock_pwm_ops, 0, sizeof(mock_pwm_ops));
}

static void mock_write_pin(void *ctx, uint8_t pin_id, bool level)
{
    (void)ctx;
    if (pin_id < MAX_PINS) {
        mock_pin_state[pin_id] = level;
    }
    mock_pin_write_count++;

    if (mock_pin_op_count < 64) {
        mock_pin_ops[mock_pin_op_count].pin_id = pin_id;
        mock_pin_ops[mock_pin_op_count].level  = level;
        mock_pin_op_count++;
    }
}

static void mock_set_pwm_duty(void *ctx, uint8_t channel, uint16_t duty)
{
    (void)ctx;
    if (channel < MAX_PINS) {
        mock_pwm_duty[channel] = duty;
    }
    mock_pwm_write_count++;

    if (mock_pwm_op_count < 64) {
        mock_pwm_ops[mock_pwm_op_count].channel = channel;
        mock_pwm_ops[mock_pwm_op_count].duty    = duty;
        mock_pwm_op_count++;
    }
}

static const tb6612_hal_t mock_hal = {
    .write_pin    = mock_write_pin,
    .set_pwm_duty = mock_set_pwm_duty,
    .ctx          = NULL,
};

/* Pin assignments for tests */
enum {
    PIN_AIN1 = 0,
    PIN_AIN2 = 1,
    PIN_BIN1 = 2,
    PIN_BIN2 = 3,
    PIN_STBY = 4,
    PWM_CHA  = 5,
    PWM_CHB  = 6,
};

static const tb6612_cfg_t test_cfg = {
    .ain1     = PIN_AIN1,
    .ain2     = PIN_AIN2,
    .bin1     = PIN_BIN1,
    .bin2     = PIN_BIN2,
    .stby     = PIN_STBY,
    .pwm_ch_a = PWM_CHA,
    .pwm_ch_b = PWM_CHB,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));

    tb6612_err_t e = tb6612_init(&dev, &mock_hal, &test_cfg);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_TRUE(dev.standby);

    /* STBY should be LOW (standby) */
    ASSERT_FALSE(mock_pin_state[PIN_STBY]);

    /* Direction pins all LOW */
    ASSERT_FALSE(mock_pin_state[PIN_AIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_AIN2]);
    ASSERT_FALSE(mock_pin_state[PIN_BIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_BIN2]);

    /* PWM at zero */
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 0);
    ASSERT_EQ(mock_pwm_duty[PWM_CHB], 0);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(tb6612_init(NULL, &mock_hal, &test_cfg), TB6612_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(tb6612_init(&dev, NULL, &test_cfg), TB6612_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(tb6612_init(&dev, &mock_hal, NULL), TB6612_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));

    tb6612_hal_t bad = {
        .write_pin    = NULL,
        .set_pwm_duty = mock_set_pwm_duty,
        .ctx          = NULL,
    };
    ASSERT_EQ(tb6612_init(&dev, &bad, &test_cfg), TB6612_ERR_PARAM);
}

TEST(test_forward_a)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);
    mock_pin_op_count = 0;
    mock_pwm_op_count = 0;

    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, 500);
    ASSERT_EQ(e, TB6612_OK);

    /* Forward: AIN1=HIGH, AIN2=LOW, PWM=500 */
    ASSERT_TRUE(mock_pin_state[PIN_AIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 500);
}

TEST(test_reverse_a)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, -750);
    ASSERT_EQ(e, TB6612_OK);

    /* Reverse: AIN1=LOW, AIN2=HIGH, PWM=750 */
    ASSERT_FALSE(mock_pin_state[PIN_AIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 750);
}

TEST(test_coast_a)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_A, 0);
    ASSERT_EQ(e, TB6612_OK);

    /* Coast: AIN1=LOW, AIN2=LOW, PWM=0 */
    ASSERT_FALSE(mock_pin_state[PIN_AIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 0);
}

TEST(test_forward_b)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_B, 300);
    ASSERT_EQ(e, TB6612_OK);

    /* Forward B: BIN1=HIGH, BIN2=LOW */
    ASSERT_TRUE(mock_pin_state[PIN_BIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_BIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHB], 300);
}

TEST(test_reverse_b)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_set_speed(&dev, TB6612_MOTOR_B, -600);
    ASSERT_EQ(e, TB6612_OK);

    ASSERT_FALSE(mock_pin_state[PIN_BIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_BIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHB], 600);
}

TEST(test_speed_clamped)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    /* Over 1000 should clamp */
    tb6612_set_speed(&dev, TB6612_MOTOR_A, 1500);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 1000);

    /* Under -1000 should clamp */
    tb6612_set_speed(&dev, TB6612_MOTOR_A, -1500);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 1000);
}

TEST(test_brake_a)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_brake(&dev, TB6612_MOTOR_A);
    ASSERT_EQ(e, TB6612_OK);

    /* Brake: both HIGH */
    ASSERT_TRUE(mock_pin_state[PIN_AIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_AIN2]);
}

TEST(test_brake_b)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_err_t e = tb6612_brake(&dev, TB6612_MOTOR_B);
    ASSERT_EQ(e, TB6612_OK);

    ASSERT_TRUE(mock_pin_state[PIN_BIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_BIN2]);
}

TEST(test_standby_enable)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    /* Enable standby = STBY LOW */
    tb6612_err_t e = tb6612_standby(&dev, true);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_FALSE(mock_pin_state[PIN_STBY]);
    ASSERT_TRUE(dev.standby);
}

TEST(test_standby_disable)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    /* Disable standby = STBY HIGH (active) */
    tb6612_err_t e = tb6612_standby(&dev, false);
    ASSERT_EQ(e, TB6612_OK);
    ASSERT_TRUE(mock_pin_state[PIN_STBY]);
    ASSERT_FALSE(dev.standby);
}

TEST(test_invalid_motor)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    ASSERT_EQ(tb6612_set_speed(&dev, (tb6612_motor_t)5, 500), TB6612_ERR_PARAM);
    ASSERT_EQ(tb6612_brake(&dev, (tb6612_motor_t)5), TB6612_ERR_PARAM);
}

TEST(test_set_speed_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(tb6612_set_speed(NULL, TB6612_MOTOR_A, 500), TB6612_ERR_PARAM);
}

TEST(test_brake_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(tb6612_brake(NULL, TB6612_MOTOR_A), TB6612_ERR_PARAM);
}

TEST(test_standby_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(tb6612_standby(NULL, true), TB6612_ERR_PARAM);
}

TEST(test_full_speed_forward)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_set_speed(&dev, TB6612_MOTOR_A, 1000);
    ASSERT_TRUE(mock_pin_state[PIN_AIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 1000);
}

TEST(test_full_speed_reverse)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_set_speed(&dev, TB6612_MOTOR_A, -1000);
    ASSERT_FALSE(mock_pin_state[PIN_AIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 1000);
}

TEST(test_both_motors_independent)
{
    mock_reset_all();
    tb6612_t dev;
    memset(&dev, 0, sizeof(dev));
    tb6612_init(&dev, &mock_hal, &test_cfg);

    tb6612_set_speed(&dev, TB6612_MOTOR_A, 500);
    tb6612_set_speed(&dev, TB6612_MOTOR_B, -300);

    /* A: forward 500 */
    ASSERT_TRUE(mock_pin_state[PIN_AIN1]);
    ASSERT_FALSE(mock_pin_state[PIN_AIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHA], 500);

    /* B: reverse 300 */
    ASSERT_FALSE(mock_pin_state[PIN_BIN1]);
    ASSERT_TRUE(mock_pin_state[PIN_BIN2]);
    ASSERT_EQ(mock_pwm_duty[PWM_CHB], 300);
}

TEST_MAIN("tb6612")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_null_cfg);
    RUN(test_init_null_callbacks);
    RUN(test_forward_a);
    RUN(test_reverse_a);
    RUN(test_coast_a);
    RUN(test_forward_b);
    RUN(test_reverse_b);
    RUN(test_speed_clamped);
    RUN(test_brake_a);
    RUN(test_brake_b);
    RUN(test_standby_enable);
    RUN(test_standby_disable);
    RUN(test_invalid_motor);
    RUN(test_set_speed_null_dev);
    RUN(test_brake_null_dev);
    RUN(test_standby_null_dev);
    RUN(test_full_speed_forward);
    RUN(test_full_speed_reverse);
    RUN(test_both_motors_independent);
TEST_MAIN_END()
