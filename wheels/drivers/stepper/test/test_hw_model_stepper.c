// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_stepper.h"

/* ---------- tests ---------- */

TEST(test_single_step_cw)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_step_once(&s, STEPPER_DIR_CW);
    ASSERT_EQ(stepper_model_get_steps(&model), 1);
    ASSERT_EQ(stepper_model_get_total_steps(&model), 1);
    ASSERT_EQ(stepper_model_get_direction(&model), STEPPER_MODEL_DIR_CW);
}

TEST(test_single_step_ccw)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_step_once(&s, STEPPER_DIR_CCW);
    ASSERT_EQ(stepper_model_get_steps(&model), -1);
    ASSERT_EQ(stepper_model_get_total_steps(&model), 1);
    ASSERT_EQ(stepper_model_get_direction(&model), STEPPER_MODEL_DIR_CCW);
}

TEST(test_move_forward_100)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_move(&s, 100);
    ASSERT_EQ(stepper_model_get_steps(&model), 100);
    ASSERT_EQ(stepper_model_get_total_steps(&model), 100);
    ASSERT_EQ(stepper_get_position(&s), 100);
}

TEST(test_move_backward_50)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_move(&s, -50);
    ASSERT_EQ(stepper_model_get_steps(&model), -50);
    ASSERT_EQ(stepper_model_get_total_steps(&model), 50);
    ASSERT_EQ(stepper_model_get_direction(&model), STEPPER_MODEL_DIR_CCW);
    ASSERT_EQ(stepper_get_position(&s), -50);
}

TEST(test_forward_then_backward_net_zero)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_move(&s, 50);
    stepper_move(&s, -50);
    ASSERT_EQ(stepper_model_get_steps(&model), 0);
    ASSERT_EQ(stepper_model_get_total_steps(&model), 100);
    ASSERT_EQ(stepper_get_position(&s), 0);
}

TEST(test_accel_phase_delays_decrease)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 500, 200);

    stepper_move(&s, 100);
    /* First several events should have decreasing delays (acceleration) */
    uint32_t count = stepper_model_get_event_count(&model);
    ASSERT_TRUE(count > 5);
    uint32_t check_end = count < 10 ? count : 10;
    ASSERT_TRUE(stepper_model_delays_decreasing(&model, 0, check_end));
}

TEST(test_decel_phase_delays_increase)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 500, 200);

    stepper_move(&s, 100);
    uint32_t count = stepper_model_get_event_count(&model);
    ASSERT_TRUE(count > 10);
    /* Last few events before the final step (which has delay_us=0) should
     * have increasing delays.  The very last event has delay_us=0 because
     * the driver does not delay after the final step, so check up to
     * count-1 (the last event with a real delay). */
    uint32_t end = count - 1;  /* exclude last event (delay=0) */
    uint32_t start = end > 5 ? end - 5 : 0;
    ASSERT_TRUE(stepper_model_delays_increasing(&model, start, end));
}

TEST(test_move_to_absolute)
{
    stepper_model_t model;
    stepper_model_init(&model);
    stepper_hal_t hal = stepper_model_hal(&model);
    stepper_t s;
    stepper_init(&s, &hal, 1000, 500);

    stepper_move_to(&s, 75);
    ASSERT_EQ(stepper_model_get_steps(&model), 75);
    ASSERT_EQ(stepper_get_position(&s), 75);

    /* move_to 25: should go backward 50 steps */
    stepper_move_to(&s, 25);
    /* net from start: 75 - 50 = 25 */
    ASSERT_EQ(stepper_model_get_steps(&model), 25);
    ASSERT_EQ(stepper_get_position(&s), 25);
}

TEST_MAIN("stepper_hw_model")
    RUN(test_single_step_cw);
    RUN(test_single_step_ccw);
    RUN(test_move_forward_100);
    RUN(test_move_backward_50);
    RUN(test_forward_then_backward_net_zero);
    RUN(test_accel_phase_delays_decrease);
    RUN(test_decel_phase_delays_increase);
    RUN(test_move_to_absolute);
TEST_MAIN_END()
