#include "test_harness.h"
#include "debounce.h"

TEST(test_init)
{
    debounce_t d;
    debounce_init(&d, 3, false);
    ASSERT_FALSE(debounce_get(&d));
    ASSERT_FALSE(debounce_changed(&d));
}

TEST(test_stable_after_threshold)
{
    debounce_t d;
    debounce_init(&d, 3, false);
    ASSERT_FALSE(debounce_update(&d, true));
    ASSERT_FALSE(debounce_changed(&d));
    ASSERT_FALSE(debounce_update(&d, true));
    ASSERT_FALSE(debounce_changed(&d));
    ASSERT_TRUE(debounce_update(&d, true));
    ASSERT_TRUE(debounce_changed(&d));
    ASSERT_TRUE(debounce_rising(&d));
    ASSERT_FALSE(debounce_falling(&d));
}

TEST(test_bounce_rejection)
{
    debounce_t d;
    debounce_init(&d, 3, false);
    debounce_update(&d, true);
    debounce_update(&d, true);
    debounce_update(&d, false);
    debounce_update(&d, true);
    debounce_update(&d, true);
    ASSERT_FALSE(debounce_get(&d));
}

TEST(test_falling_edge)
{
    debounce_t d;
    debounce_init(&d, 2, true);
    debounce_update(&d, false);
    ASSERT_TRUE(debounce_get(&d));
    debounce_update(&d, false);
    ASSERT_FALSE(debounce_get(&d));
    ASSERT_TRUE(debounce_falling(&d));
    ASSERT_FALSE(debounce_rising(&d));
}

TEST(test_no_change_when_stable)
{
    debounce_t d;
    debounce_init(&d, 2, false);
    debounce_update(&d, false);
    debounce_update(&d, false);
    debounce_update(&d, false);
    ASSERT_FALSE(debounce_changed(&d));
}

TEST(test_threshold_1)
{
    debounce_t d;
    debounce_init(&d, 1, false);
    ASSERT_TRUE(debounce_update(&d, true));
    ASSERT_TRUE(debounce_changed(&d));
}

TEST(test_null_safety)
{
    debounce_init(NULL, 3, false);
    ASSERT_FALSE(debounce_update(NULL, true));
    ASSERT_FALSE(debounce_get(NULL));
    ASSERT_FALSE(debounce_changed(NULL));
    ASSERT_FALSE(debounce_rising(NULL));
    ASSERT_FALSE(debounce_falling(NULL));
}

TEST(test_rapid_toggle)
{
    debounce_t d;
    debounce_init(&d, 5, false);
    for (int i = 0; i < 20; i++)
        debounce_update(&d, (i % 2) != 0);
    ASSERT_FALSE(debounce_get(&d));
}

TEST_MAIN("debounce")
    RUN(test_init);
    RUN(test_stable_after_threshold);
    RUN(test_bounce_rejection);
    RUN(test_falling_edge);
    RUN(test_no_change_when_stable);
    RUN(test_threshold_1);
    RUN(test_null_safety);
    RUN(test_rapid_toggle);
TEST_MAIN_END()
