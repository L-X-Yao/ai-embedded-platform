#include "test_harness.h"
#include "led_pattern.h"

TEST(test_simple_blink)
{
    led_step_t steps[] = {{.on_ticks = 2, .off_ticks = 2}};
    led_pattern_cfg_t cfg = {.steps = steps, .step_count = 1, .loop = true};
    led_pattern_t p;
    led_pattern_init(&p, &cfg);
    ASSERT_TRUE(led_pattern_is_on(&p));
    ASSERT_TRUE(led_pattern_tick(&p));   /* tick 1: on */
    ASSERT_TRUE(led_pattern_tick(&p));   /* tick 2: on->off transition */
    ASSERT_FALSE(led_pattern_tick(&p));  /* tick 3: off */
    ASSERT_FALSE(led_pattern_tick(&p));  /* tick 4: off->loop */
    ASSERT_TRUE(led_pattern_tick(&p));   /* tick 5: on again */
}

TEST(test_one_shot)
{
    led_step_t steps[] = {{.on_ticks = 2, .off_ticks = 1}};
    led_pattern_cfg_t cfg = {.steps = steps, .step_count = 1, .loop = false};
    led_pattern_t p;
    led_pattern_init(&p, &cfg);
    ASSERT_FALSE(led_pattern_finished(&p));
    led_pattern_tick(&p);
    led_pattern_tick(&p);
    led_pattern_tick(&p);
    ASSERT_TRUE(led_pattern_finished(&p));
}

TEST(test_multi_step)
{
    led_step_t steps[] = {
        {.on_ticks = 1, .off_ticks = 1},
        {.on_ticks = 3, .off_ticks = 1}
    };
    led_pattern_cfg_t cfg = {.steps = steps, .step_count = 2, .loop = false};
    led_pattern_t p;
    led_pattern_init(&p, &cfg);
    ASSERT_TRUE(led_pattern_tick(&p));   /* step 0 on */
    ASSERT_FALSE(led_pattern_tick(&p));  /* step 0 off */
    ASSERT_TRUE(led_pattern_tick(&p));   /* step 1 on */
    ASSERT_TRUE(led_pattern_tick(&p));   /* step 1 on */
    ASSERT_TRUE(led_pattern_tick(&p));   /* step 1 on->off */
    ASSERT_FALSE(led_pattern_tick(&p));  /* step 1 off -> finished */
    ASSERT_TRUE(led_pattern_finished(&p));
}

TEST(test_restart)
{
    led_step_t steps[] = {{.on_ticks = 1, .off_ticks = 1}};
    led_pattern_cfg_t cfg = {.steps = steps, .step_count = 1, .loop = false};
    led_pattern_t p;
    led_pattern_init(&p, &cfg);
    led_pattern_tick(&p);
    led_pattern_tick(&p);
    ASSERT_TRUE(led_pattern_finished(&p));
    led_pattern_restart(&p);
    ASSERT_FALSE(led_pattern_finished(&p));
    ASSERT_TRUE(led_pattern_is_on(&p));
}

TEST(test_empty_pattern)
{
    led_pattern_cfg_t cfg = {.steps = NULL, .step_count = 0, .loop = false};
    led_pattern_t p;
    led_pattern_init(&p, &cfg);
    ASSERT_TRUE(led_pattern_finished(&p));
    ASSERT_FALSE(led_pattern_tick(&p));
}

TEST(test_null_safety)
{
    led_pattern_init(NULL, NULL);
    ASSERT_FALSE(led_pattern_tick(NULL));
    ASSERT_FALSE(led_pattern_is_on(NULL));
    ASSERT_TRUE(led_pattern_finished(NULL));
    led_pattern_restart(NULL);
}

TEST_MAIN("led_pattern")
    RUN(test_simple_blink);
    RUN(test_one_shot);
    RUN(test_multi_step);
    RUN(test_restart);
    RUN(test_empty_pattern);
    RUN(test_null_safety);
TEST_MAIN_END()
