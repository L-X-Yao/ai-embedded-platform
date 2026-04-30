#include "test_harness.h"
#include "rotary_encoder.h"

TEST(test_init)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_NONE);
}

TEST(test_cw_full_cycle)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc);
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
    rotary_encoder_init(&enc);
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
    rotary_encoder_init(&enc);
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
    rotary_encoder_init(&enc);
    rotary_encoder_update(&enc, false, false);
    rotary_encoder_update(&enc, false, false);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
}

TEST(test_reset)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc);
    rotary_encoder_update(&enc, false, true);
    rotary_encoder_update(&enc, true,  true);
    rotary_encoder_reset(&enc);
    ASSERT_EQ(rotary_encoder_get_position(&enc), 0);
    ASSERT_EQ(rotary_encoder_get_direction(&enc), ROTARY_ENCODER_DIR_NONE);
}

TEST(test_null_safety)
{
    rotary_encoder_init(NULL);
    rotary_encoder_update(NULL, false, false);
    ASSERT_EQ(rotary_encoder_get_position(NULL), 0);
    ASSERT_EQ(rotary_encoder_get_direction(NULL), ROTARY_ENCODER_DIR_NONE);
    rotary_encoder_reset(NULL);
}

TEST(test_many_steps)
{
    rotary_encoder_t enc;
    rotary_encoder_init(&enc);
    for (int i = 0; i < 100; i++) {
        rotary_encoder_update(&enc, false, true);
        rotary_encoder_update(&enc, true,  true);
        rotary_encoder_update(&enc, true,  false);
        rotary_encoder_update(&enc, false, false);
    }
    ASSERT_EQ(rotary_encoder_get_position(&enc), 400);
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
TEST_MAIN_END()
