// SPDX-License-Identifier: MIT
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
    ASSERT_EQ(stepper_init(NULL, &mock_hal, 1000, 500), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_init(&s, NULL, 1000, 500), STEPPER_ERR_PARAM);
}

TEST(test_init_zero_speed)
{
    stepper_t s;
    ASSERT_EQ(stepper_init(&s, &mock_hal, 0, 500), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_init(&s, &mock_hal, 1000, 0), STEPPER_ERR_PARAM);
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

/* ---------- STEPPER_ERR_BUSY: defined for future non-blocking API --------
 * The current stepper_move() is blocking (runs full ramp before returning),
 * so there is no concurrent-busy path.  ERR_BUSY is reserved for a future
 * non-blocking / ISR-driven variant.
 *
 * These tests verify ABI stability and exercise the state field that
 * a non-blocking caller would check before issuing a move. */

TEST(test_err_busy_value)
{
    ASSERT_EQ((int)STEPPER_ERR_BUSY, -2);
}

TEST(test_state_idle_after_move)
{
    /* After a blocking move completes, state must be IDLE so that a
     * future non-blocking implementation can use state != IDLE to
     * return ERR_BUSY. */
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_move(&s, 50);
    ASSERT_EQ(stepper_get_state(&s), STEPPER_STATE_IDLE);
}

TEST(test_state_idle_after_move_to)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_move_to(&s, 100);
    ASSERT_EQ(stepper_get_state(&s), STEPPER_STATE_IDLE);
    ASSERT_EQ(stepper_get_position(&s), 100);
}

TEST(test_move_null_returns_param)
{
    /* Verify that NULL stepper returns PARAM, not BUSY */
    ASSERT_EQ(stepper_move(NULL, 10), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_move_to(NULL, 10), STEPPER_ERR_PARAM);
}

/* ---------- ramp shape ----------
 * Expected values come from constant acceleration, not from the driver.
 * After n steps from rest v = sqrt(2*a*n), so the delay there is
 * 1e6 / sqrt(2*a*n) us, and Austin's first delay is
 * 0.676 * sqrt(2/a) * 1e6 us. */

#define RAMP_MAX 40000U

static uint32_t g_ramp[RAMP_MAX];   /* every g_ramp_stride-th step delay */
static uint32_t g_ramp_stride;
static uint32_t g_ramp_n;           /* step delays seen */
static uint32_t g_ramp_last;        /* the latest step delay */

static void ramp_step_pin(void *ctx, bool level)
{
    (void)ctx;
    (void)level;
}

static void ramp_dir_pin(void *ctx, bool level)
{
    (void)ctx;
    (void)level;
}

static void ramp_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    if (us <= 2) return;   /* pulse width, not a step delay */
    if (g_ramp_n % g_ramp_stride == 0 &&
        g_ramp_n / g_ramp_stride < RAMP_MAX) {
        g_ramp[g_ramp_n / g_ramp_stride] = us;
    }
    g_ramp_last = us;
    g_ramp_n++;
}

static const stepper_hal_t ramp_hal = {
    .step_pin  = ramp_step_pin,
    .dir_pin   = ramp_dir_pin,
    .delay_us  = ramp_delay_us,
    .ctx       = NULL,
};

static void ramp_run(uint16_t max_speed, uint16_t accel, int32_t steps,
                     uint32_t stride)
{
    stepper_t s;
    g_ramp_n = 0;
    g_ramp_stride = stride;
    stepper_init(&s, &ramp_hal, max_speed, accel);
    stepper_move(&s, steps);
}

static bool within_pct(uint32_t got, uint32_t want, uint32_t pct)
{
    uint32_t diff = got > want ? got - want : want - got;
    return (uint64_t)diff * 100U <= (uint64_t)want * pct;
}

TEST(test_ramp_up_follows_constant_accel)
{
    /* 5000 sps at 1000 sps^2 takes 12500 steps. */
    ramp_run(5000, 1000, 40000, 1);
    ASSERT_EQ(g_ramp_n, 39999);
    ASSERT_TRUE(within_pct(g_ramp[500], 1000, 3));    /* 1000 sps */
    ASSERT_TRUE(within_pct(g_ramp[2000], 500, 3));    /* 2000 sps */
    ASSERT_TRUE(within_pct(g_ramp[4500], 333, 3));    /* 3000 sps */
    ASSERT_TRUE(within_pct(g_ramp[8000], 250, 3));    /* 4000 sps */
    ASSERT_EQ(g_ramp[20000], 200);                    /* cruise */
}

TEST(test_ramp_has_no_speed_jump)
{
    ramp_run(5000, 1000, 40000, 1);
    /* Neighbouring delays differ by about 1/(2n) on a true ramp. */
    for (uint32_t i = 20; i + 20 < g_ramp_n; i++) {
        ASSERT_TRUE(within_pct(g_ramp[i], g_ramp[i - 1], 5));
    }
}

TEST(test_ramp_down_mirrors_ramp_up)
{
    ramp_run(5000, 1000, 40000, 1);
    uint32_t last = g_ramp_n - 1;
    ASSERT_TRUE(within_pct(g_ramp[last - 8000], 250, 3));
    ASSERT_TRUE(within_pct(g_ramp[last - 4500], 333, 3));
    ASSERT_TRUE(within_pct(g_ramp[last - 2000], 500, 3));
    ASSERT_TRUE(within_pct(g_ramp[last - 500], 1000, 3));
    /* It must stop from the speed it started at: 0.676*sqrt(2/1000). */
    ASSERT_TRUE(within_pct(g_ramp[last], 30231, 5));
}

TEST(test_ramp_low_accel)
{
    /* 125000 steps to reach 5000 sps at 100 sps^2; 2500 sps is at 31250. */
    ramp_run(5000, 100, 260000, 10);
    ASSERT_EQ(g_ramp_n, 259999);
    ASSERT_TRUE(within_pct(g_ramp[3125], 400, 3));
    ASSERT_TRUE(within_pct(g_ramp[12000], 204, 3));   /* 4899 sps */
    ASSERT_TRUE(within_pct(g_ramp[25999 - 3125], 400, 3));
}

TEST(test_ramp_extreme_range)
{
    /* At the slowest accel, 1 sps^2, 2000 sps takes 2000000 steps and the
     * first delay is near 1 s. */
    ramp_run(2000, 1, 4400000, 500);
    ASSERT_EQ(g_ramp_n, 4399999);
    ASSERT_TRUE(within_pct(g_ramp[0], 956008, 3));    /* 0.676*sqrt(2) */
    ASSERT_TRUE(within_pct(g_ramp[1000], 1000, 3));   /* n=500000 */
    ASSERT_EQ(g_ramp[4400], 500);                     /* cruise */
    ASSERT_TRUE(within_pct(g_ramp[8799 - 1000], 1000, 3));
    ASSERT_TRUE(within_pct(g_ramp_last, 956008, 5));
}

TEST(test_move_to_refuses_unreachable_distance)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_set_position(&s, -2000000000);
    ASSERT_EQ(stepper_move_to(&s, 2000000000), STEPPER_ERR_PARAM);
    stepper_set_position(&s, 2000000000);
    ASSERT_EQ(stepper_move_to(&s, -2000000000), STEPPER_ERR_PARAM);
    /* One past the int32_t range in each direction. */
    stepper_set_position(&s, -1);
    ASSERT_EQ(stepper_move_to(&s, INT32_MAX), STEPPER_ERR_PARAM);
    stepper_set_position(&s, 1);
    ASSERT_EQ(stepper_move_to(&s, INT32_MIN), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_get_position(&s), 1);
    ASSERT_EQ(g_mock.step_count, 0);
}

TEST(test_move_to_near_the_limits)
{
    stepper_t s;
    stepper_init(&s, &mock_hal, 1000, 500);
    mock_reset();
    stepper_set_position(&s, INT32_MAX - 5);
    ASSERT_EQ(stepper_move_to(&s, INT32_MAX), STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&s), INT32_MAX);
    ASSERT_FALSE(g_mock.last_dir);
    stepper_set_position(&s, INT32_MIN + 5);
    ASSERT_EQ(stepper_move_to(&s, INT32_MIN), STEPPER_OK);
    ASSERT_EQ(stepper_get_position(&s), INT32_MIN);
    ASSERT_TRUE(g_mock.last_dir);
    ASSERT_EQ(g_mock.step_count, 10);
}

TEST(test_triangle_3_steps_stays_slow)
{
    /* Too short to accelerate: the middle step must not run at 5000 sps. */
    ramp_run(5000, 1000, 3, 1);
    ASSERT_EQ(g_ramp_n, 2);
    ASSERT_TRUE(within_pct(g_ramp[0], 30231, 5));
    ASSERT_EQ(g_ramp[1], g_ramp[0]);
}

TEST(test_triangle_odd_peaks_on_the_ramp)
{
    /* 500 steps up, one at the peak, 500 down, so 1000 delays.  The peak
     * is 1e6 / sqrt(2*1000*500) = 1000 us. */
    ramp_run(5000, 1000, 1001, 1);
    ASSERT_EQ(g_ramp_n, 1000);
    ASSERT_TRUE(within_pct(g_ramp[499], 1000, 3));
    ASSERT_EQ(g_ramp[500], g_ramp[499]);
    for (uint32_t i = 20; i < 980; i++) {
        ASSERT_TRUE(within_pct(g_ramp[i], g_ramp[i - 1], 5));
    }
    for (uint32_t i = 0; i < 500; i++) {
        ASSERT_TRUE(within_pct(g_ramp[999 - i], g_ramp[i], 3));
    }
}

TEST(test_triangle_even_peaks_on_the_ramp)
{
    /* 500 steps up and 500 down give 999 delays, the middle one the peak. */
    ramp_run(5000, 1000, 1000, 1);
    ASSERT_EQ(g_ramp_n, 999);
    ASSERT_TRUE(within_pct(g_ramp[499], 1000, 3));
    for (uint32_t i = 0; i < 499; i++) {
        ASSERT_TRUE(within_pct(g_ramp[998 - i], g_ramp[i], 3));
    }
}

/* ---------- a device init refused ---------- */

static void assert_device_refused(stepper_t *s)
{
    mock_reset();
    ASSERT_EQ(stepper_move(s, 10), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_move(s, 0), STEPPER_ERR_PARAM);
    ASSERT_EQ(stepper_move_to(s, 10), STEPPER_ERR_PARAM);
    stepper_step_once(s, STEPPER_DIR_CW);
    ASSERT_EQ(stepper_get_position(s), 0);
    ASSERT_EQ(stepper_get_state(s), STEPPER_STATE_IDLE);
    ASSERT_EQ(g_mock.step_count, 0);
}

TEST(test_zeroed_device_refused)
{
    static stepper_t zeroed;
    assert_device_refused(&zeroed);
}

/* s holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    stepper_t s;
    stepper_hal_t no_step = mock_hal;
    no_step.step_pin = NULL;

    memset(&s, 0xA5, sizeof(s));
    ASSERT_EQ(stepper_init(&s, NULL, 1000, 500), STEPPER_ERR_PARAM);
    assert_device_refused(&s);

    memset(&s, 0xA5, sizeof(s));
    ASSERT_EQ(stepper_init(&s, &no_step, 1000, 500), STEPPER_ERR_PARAM);
    assert_device_refused(&s);

    memset(&s, 0xA5, sizeof(s));
    ASSERT_EQ(stepper_init(&s, &mock_hal, 0, 500), STEPPER_ERR_PARAM);
    assert_device_refused(&s);

    memset(&s, 0xA5, sizeof(s));
    ASSERT_EQ(stepper_init(&s, &mock_hal, 1000, 0), STEPPER_ERR_PARAM);
    assert_device_refused(&s);
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
    RUN(test_err_busy_value);
    RUN(test_state_idle_after_move);
    RUN(test_state_idle_after_move_to);
    RUN(test_move_null_returns_param);
    RUN(test_ramp_up_follows_constant_accel);
    RUN(test_ramp_has_no_speed_jump);
    RUN(test_ramp_down_mirrors_ramp_up);
    RUN(test_ramp_low_accel);
    RUN(test_ramp_extreme_range);
    RUN(test_move_to_refuses_unreachable_distance);
    RUN(test_move_to_near_the_limits);
    RUN(test_triangle_3_steps_stays_slow);
    RUN(test_triangle_odd_peaks_on_the_ramp);
    RUN(test_triangle_even_peaks_on_the_ramp);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
