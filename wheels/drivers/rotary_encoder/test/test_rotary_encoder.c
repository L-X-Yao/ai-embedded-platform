#include "test_harness.h"
#include "rotary_encoder.h"

/* ---------- mock HAL ---------- */

static bool mock_pin_a;
static bool mock_pin_b;

static bool mock_read_a(void *ctx)
{
    (void)ctx;
    return mock_pin_a;
}

static bool mock_read_b(void *ctx)
{
    (void)ctx;
    return mock_pin_b;
}

static const rotary_encoder_hal_t mock_hal = {
    .read_a = mock_read_a,
    .read_b = mock_read_b,
    .ctx    = NULL,
};

/* ---------- tests ---------- */

TEST(test_init)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_NONE);
}

TEST(test_cw_full_cycle)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    /* CW sequence: 00->01->11->10->00 */
    rotary_encoder_update(&enc, false, true);   /* 00->01 */
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_CW);
    rotary_encoder_update(&enc, true,  true);   /* 01->11 */
    rotary_encoder_update(&enc, true,  false);  /* 11->10 */
    rotary_encoder_update(&enc, false, false);  /* 10->00 */
    ASSERT_EQ(rotary_encoder_get_position(&enc), 4);
}

TEST(test_ccw_full_cycle)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    /* CCW sequence: 00->10->11->01->00 */
    rotary_encoder_update(&enc, true,  false);  /* 00->10 */
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_CCW);
    rotary_encoder_update(&enc, true,  true);   /* 10->11 */
    rotary_encoder_update(&enc, false, true);   /* 11->01 */
    rotary_encoder_update(&enc, false, false);  /* 01->00 */
    ASSERT_EQ(rotary_encoder_get_position(&enc), -4);
}

TEST(test_mixed_direction)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    rotary_encoder_update(&enc, false, true);
    rotary_encoder_update(&enc, true,  true);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 2);
    rotary_encoder_update(&enc, false, true);
    rotary_encoder_update(&enc, false, false);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
}

TEST(test_same_state_no_change)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    rotary_encoder_update(&enc, false, false);
    rotary_encoder_update(&enc, false, false);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
}

TEST(test_reset)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    rotary_encoder_update(&enc, false, true);
    rotary_encoder_update(&enc, true,  true);
    rotary_encoder_reset(&enc);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_NONE);
}

TEST(test_null_safety)
{
    rotary_encoder_init(NULL, &mock_hal);
    rotary_encoder_update(NULL, false, false);
    rotary_encoder_poll(NULL);
    ASSERT_EQ(rotary_encoder_get_position(NULL), 0);
    ASSERT_EQ(rotary_encoder_get_direction(NULL), ROTARY_ENCODER_DIR_NONE);
    rotary_encoder_reset(NULL);
}

TEST(test_many_steps)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    for (int i = 0; i < 100; i++) {
        rotary_encoder_update(&enc, false, true);
        rotary_encoder_update(&enc, true,  true);
        rotary_encoder_update(&enc, true,  false);
        rotary_encoder_update(&enc, false, false);
    }
    ASSERT_EQ(rotary_encoder_get_position(&enc), 400);
}

TEST(test_poll_cw)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &mock_hal);
    /* CW via poll: 00->01->11->10->00 */
    mock_pin_a = false; mock_pin_b = true;
    rotary_encoder_poll(&enc);
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_CW);

    mock_pin_a = true;  mock_pin_b = true;
    rotary_encoder_poll(&enc);
    mock_pin_a = true;  mock_pin_b = false;
    rotary_encoder_poll(&enc);
    mock_pin_a = false; mock_pin_b = false;
    rotary_encoder_poll(&enc);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 4);
}

TEST(test_poll_null_hal)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, NULL);
    /* poll with NULL hal should not crash */
    rotary_encoder_poll(&enc);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
}

TEST_MAIN("rotary_encoder")
    RUN(test_init);
    RUN(test_cw_full_cycle);
    RUN(test_ccw_full_cycle);
    RUN(test_mixed_direction);
    RUN(test_same_state_no_change);
    RUN(test_reset);
    RUN(test_null_safety);
    RUN(test_many_steps);
    RUN(test_poll_cw);
    RUN(test_poll_null_hal);
TEST_MAIN_END()
