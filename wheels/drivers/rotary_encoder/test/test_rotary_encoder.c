#include "rotary_encoder.h"
#include <stdio.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) static void name(void)
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL %s:%d: %d != %d\n", __func__, __LINE__, \
               (int)(a), (int)(b)); \
        g_fail++; return; \
    } } while(0)
#define RUN(fn) do { int pf = g_fail; fn(); if(g_fail == pf) { \
    printf("  PASS %s\n", #fn); g_pass++; } } while(0)

TEST(test_init)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    ASSERT_EQ(encoder_get_position(&enc), 0);
    ASSERT_EQ(encoder_get_direction(&enc), ENCODER_DIR_NONE);
}

TEST(test_cw_full_cycle)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    /* CW sequence: 00→01→11→10→00 */
    encoder_update(&enc, false, true);   /* 00→01 */
    ASSERT_EQ(encoder_get_direction(&enc), ENCODER_DIR_CW);
    encoder_update(&enc, true,  true);   /* 01→11 */
    encoder_update(&enc, true,  false);  /* 11→10 */
    encoder_update(&enc, false, false);  /* 10→00 */
    ASSERT_EQ(encoder_get_position(&enc), 4);
}

TEST(test_ccw_full_cycle)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    /* CCW sequence: 00→10→11→01→00 */
    encoder_update(&enc, true,  false);  /* 00→10 */
    ASSERT_EQ(encoder_get_direction(&enc), ENCODER_DIR_CCW);
    encoder_update(&enc, true,  true);   /* 10→11 */
    encoder_update(&enc, false, true);   /* 11→01 */
    encoder_update(&enc, false, false);  /* 01→00 */
    ASSERT_EQ(encoder_get_position(&enc), -4);
}

TEST(test_mixed_direction)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    encoder_update(&enc, false, true);
    encoder_update(&enc, true,  true);
    ASSERT_EQ(encoder_get_position(&enc), 2);
    encoder_update(&enc, false, true);
    encoder_update(&enc, false, false);
    ASSERT_EQ(encoder_get_position(&enc), 0);
}

TEST(test_same_state_no_change)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    encoder_update(&enc, false, false);
    encoder_update(&enc, false, false);
    ASSERT_EQ(encoder_get_position(&enc), 0);
}

TEST(test_reset)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    encoder_update(&enc, false, true);
    encoder_update(&enc, true,  true);
    encoder_reset(&enc);
    ASSERT_EQ(encoder_get_position(&enc), 0);
    ASSERT_EQ(encoder_get_direction(&enc), ENCODER_DIR_NONE);
}

TEST(test_null_safety)
{
    encoder_init(NULL);
    encoder_update(NULL, false, false);
    ASSERT_EQ(encoder_get_position(NULL), 0);
    ASSERT_EQ(encoder_get_direction(NULL), ENCODER_DIR_NONE);
    encoder_reset(NULL);
}

TEST(test_many_steps)
{
    rotary_encoder_t enc;
    encoder_init(&enc);
    for (int i = 0; i < 100; i++) {
        encoder_update(&enc, false, true);
        encoder_update(&enc, true,  true);
        encoder_update(&enc, true,  false);
        encoder_update(&enc, false, false);
    }
    ASSERT_EQ(encoder_get_position(&enc), 400);
}

int main(void)
{
    printf("rotary_encoder tests:\n");
    RUN(test_init);
    RUN(test_cw_full_cycle);
    RUN(test_ccw_full_cycle);
    RUN(test_mixed_direction);
    RUN(test_same_state_no_change);
    RUN(test_reset);
    RUN(test_null_safety);
    RUN(test_many_steps);
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
