// SPDX-License-Identifier: MIT
/*
 * prop_stepper.c — property-based tests for stepper motor driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "stepper.h"

static int mock_step_count;
static bool mock_dir_level;

static void mock_step_pin(void *ctx, bool level) { (void)ctx; if (level) mock_step_count++; }
static void mock_dir_pin(void *ctx, bool level)  { (void)ctx; mock_dir_level = level; }
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const stepper_hal_t prop_hal = {
    .step_pin  = mock_step_pin,
    .dir_pin   = mock_dir_pin,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_step_count = 0;
    stepper_t s;
    uint16_t speed = (uint16_t)(100 + ARBI_UINT16(seed) % 5000);
    uint16_t accel = (uint16_t)(100 + ARBI_UINT16(seed) % 5000);
    stepper_init(&s, &prop_hal, speed, accel);
}

PROPERTY(move_tracks_position, 5000)
{
    mock_step_count = 0;
    stepper_t s;
    stepper_init(&s, &prop_hal, 1000, 500);

    /* Move small random amount to keep test fast */
    int32_t steps = (int32_t)((int8_t)ARBI_UINT8(seed));
    int32_t pos_before = stepper_get_position(&s);
    stepper_move(&s, steps);
    int32_t pos_after = stepper_get_position(&s);

    PROP_ASSERT(pos_after == pos_before + steps);
}

PROPERTY(step_once_no_crash, 5000)
{
    mock_step_count = 0;
    stepper_t s;
    stepper_init(&s, &prop_hal, 1000, 500);

    stepper_dir_t dir = (ARBI_UINT8(seed) & 1) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
    stepper_step_once(&s, dir);
    PROP_ASSERT(stepper_get_state(&s) == STEPPER_STATE_IDLE);
}

TEST_MAIN("prop_stepper")
    RUN(init_no_crash);
    RUN(move_tracks_position);
    RUN(step_once_no_crash);
TEST_MAIN_END()
