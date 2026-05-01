#include "test_harness.h"
#include "stepper.h"

/* ---------- mock HAL ---------- */

typedef struct {
    uint32_t step_count;
    bool     last_dir;
    uint32_t delay_calls;
    uint32_t delays[512];      /* record delays for ramp analysis */
    bool     last_step_level;
} mock_ctx_t;

static mock_ctx_t g_mock;

static void mock_step_pin(void *ctx, bool level)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    if (level && !m->last_step_level) {
        m->step_count++;      /* count rising edges */
    }
    m->last_step_level = level;
}

static void mock_dir_pin(void *ctx, bool level)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    m->last_dir = level;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    /* skip pulse-width delays (2us from pulse_step) */
    if (us <= 2) return;
    if (m->delay_calls < 512) {
        m->delays[m->delay_calls] = us;
    }
    m->delay_calls++;
}

static void mock_reset(void)
{
    g_mock.step_count = 0;
    g_mock.last_dir = false;
    g_mock.delay_calls = 0;
    g_mock.last_step_level = false;
    for (int i = 0; i < 512; i++) g_mock.delays[i] = 0;
}

static const stepper_hal_t mock_hal = {
    .step_pin  = mock_step_pin,
    .dir_pin   = mock_dir_pin,
    .delay_us  = mock_delay_us,
    .ctx       = &g_mock,
};

/* ---------- tests ---------- */

TEST(test_init_valid)
{
    stepper_t s;
    ASSERT_EQ(stepper_init(&s, &mock_hal, 1000, 500), STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&s), 0);
    ASSERT_EQ(stepper_get_state(&s), STEPPER_STATE_IDLE);
}

TEST(test_init_null)
{
    stepper_t s;
    ASSERT_EQ(stepper_init(NULL, &mock_hal, 1000, 500), STEPPER_ERR_PARAMS);
    ASSERT_EQ(stepper_init(&s, NULL, 1000, 500), STEPPER_ERR_PARAMS);
}

TEST(test_init_zero_speed)
{
    stepper_t s;
    ASSERT_EQ(stepper_init(&s, &mock_hal, 0, 500), STEPPER_ERR_PARAMS);
    ASSERT_EQ(stepper_init(&s, &mock_hal, 1000, 0), STEPPER_ERR_PARAMS);
}

TEST(test_step_once_cw)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_step_once(&s, STEPPER_DIR_CW);
    ASSERT_EQ(stepper_get_position(&s), 1);
    ASSERT_EQ(g_mock.step_count, 1);
}

TEST(test_step_once_ccw)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_step_once(&s, STEPPER_DIR_CCW);
    ASSERT_EQ(stepper_get_position(&s), -1);
    ASSERT_EQ(g_mock.step_count, 1);
}

TEST(test_move_forward)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_err_t err = stepper_move(&s, 100);
    ASSERT_EQ(err, STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&s), 100);
    ASSERT_EQ(g_mock.step_count, 100);
}

TEST(test_move_backward)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_move(&s, 100);
    mock_reset();
    stepper_move(&s, -50);
    ASSERT_EQ(stepper_get_position(&s), 50);
    ASSERT_EQ(g_mock.step_count, 50);
    ASSERT_EQ(g_mock.last_dir, true); /* CCW = true */
}

TEST(test_move_to_absolute)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_move_to(&s, 200);
    ASSERT_EQ(stepper_get_position(&s), 200);
    ASSERT_EQ(g_mock.step_count, 200);
}

TEST(test_move_to_current)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_move_to(&s, 0);  /* already at 0 */
    ASSERT_EQ(stepper_get_position(&s), 0);
    ASSERT_EQ(g_mock.step_count, 0);
}

TEST(test_accel_delays_decrease)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 500, 200);
    mock_reset();
    stepper_move(&s, 100);
    /* During acceleration, delays should decrease */
    ASSERT_TRUE(g_mock.delay_calls > 0);
    /* Check first few delays are decreasing */
    bool decreasing = true;
    uint32_t limit = g_mock.delay_calls < 10 ? g_mock.delay_calls : 10;
    for (uint32_t i = 1; i < limit; i++) {
        if (g_mock.delays[i] > g_mock.delays[i - 1]) {
            decreasing = false;
            break;
        }
    }
    ASSERT_TRUE(decreasing);
}

TEST(test_decel_delays_increase)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 500, 200);
    mock_reset();
    stepper_move(&s, 100);
    /* During deceleration (last few steps), delays should increase */
    ASSERT_TRUE(g_mock.delay_calls > 5);
    /* The last few delays (before the very last step which has no delay) */
    uint32_t end = g_mock.delay_calls;
    bool increasing = true;
    uint32_t start = end > 5 ? end - 5 : 0;
    for (uint32_t i = start + 1; i < end; i++) {
        if (g_mock.delays[i] < g_mock.delays[i - 1]) {
            increasing = false;
            break;
        }
    }
    ASSERT_TRUE(increasing);
}

TEST(test_short_move_symmetric)
{
    /* Short move: fewer steps than needed for full accel.
     * Should have symmetric accel/decel (triangle profile). */
    stepper_t s;
    stepper_init(&s, &mock_hal, 2000, 100);
    mock_reset();
    stepper_move(&s, 10);
    ASSERT_EQ(stepper_get_position(&s), 10);
    ASSERT_EQ(g_mock.step_count, 10);
    /* State should return to idle */
    ASSERT_EQ(stepper_get_state(&s), STEPPER_STATE_IDLE);
}

TEST(test_set_position_no_movement)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_set_position(&s, 999);
    ASSERT_EQ(stepper_get_position(&s), 999);
    ASSERT_EQ(g_mock.step_count, 0);  /* no physical movement */
}

TEST(test_move_zero)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_err_t err = stepper_move(&s, 0);
    ASSERT_EQ(err, STEPPER_OK);
    ASSERT_EQ(g_mock.step_count, 0);
    ASSERT_EQ(stepper_get_position(&s), 0);
}

TEST_MAIN("stepper")
    RUN(test_init_valid);
    RUN(test_init_null);
    RUN(test_init_zero_speed);
    RUN(test_step_once_cw);
    RUN(test_step_once_ccw);
    RUN(test_move_forward);
    RUN(test_move_backward);
    RUN(test_move_to_absolute);
    RUN(test_move_to_current);
    RUN(test_accel_delays_decrease);
    RUN(test_decel_delays_increase);
    RUN(test_short_move_symmetric);
    RUN(test_set_position_no_movement);
    RUN(test_move_zero);
TEST_MAIN_END()
