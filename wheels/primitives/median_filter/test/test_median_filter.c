#include "test_harness.h"
#include "median_filter.h"

TEST(test_init_valid)
{
    median_filter_t f;
    ASSERT_EQ(median_filter_init(&f, 3), 0);
    ASSERT_EQ(median_filter_init(&f, MEDIAN_FILTER_MAX_SIZE), 0);
}

TEST(test_init_invalid)
{
    median_filter_t f;
    ASSERT_EQ(median_filter_init(NULL, 3), -1);
    ASSERT_EQ(median_filter_init(&f, 0), -1);
    ASSERT_EQ(median_filter_init(&f, MEDIAN_FILTER_MAX_SIZE + 1), -1);
}

TEST(test_window_3)
{
    median_filter_t f;
    median_filter_init(&f, 3);
    ASSERT_EQ(median_filter_update(&f, 10), 10);
    ASSERT_EQ(median_filter_update(&f, 20), 20);
    ASSERT_EQ(median_filter_update(&f, 5), 10);
}

TEST(test_spike_rejection)
{
    median_filter_t f;
    median_filter_init(&f, 5);
    median_filter_update(&f, 100);
    median_filter_update(&f, 101);
    median_filter_update(&f, 102);
    median_filter_update(&f, 999);
    int32_t m = median_filter_update(&f, 103);
    ASSERT_EQ(m, 102);
}

TEST(test_constant_signal)
{
    median_filter_t f;
    median_filter_init(&f, 5);
    for (int i = 0; i < 10; i++) {
        int32_t m = median_filter_update(&f, 42);
        ASSERT_EQ(m, 42);
    }
}

TEST(test_descending)
{
    median_filter_t f;
    median_filter_init(&f, 3);
    median_filter_update(&f, 30);
    median_filter_update(&f, 20);
    int32_t m = median_filter_update(&f, 10);
    ASSERT_EQ(m, 20);
}

TEST(test_negative_values)
{
    median_filter_t f;
    median_filter_init(&f, 3);
    median_filter_update(&f, -5);
    median_filter_update(&f, -10);
    int32_t m = median_filter_update(&f, -1);
    ASSERT_EQ(m, -5);
}

TEST(test_reset)
{
    median_filter_t f;
    median_filter_init(&f, 3);
    median_filter_update(&f, 100);
    median_filter_update(&f, 200);
    median_filter_reset(&f);
    ASSERT_EQ(median_filter_get(&f), 0);
    ASSERT_EQ(median_filter_update(&f, 50), 50);
}

TEST(test_get)
{
    median_filter_t f;
    median_filter_init(&f, 3);
    ASSERT_EQ(median_filter_get(&f), 0);
    median_filter_update(&f, 10);
    median_filter_update(&f, 30);
    median_filter_update(&f, 20);
    ASSERT_EQ(median_filter_get(&f), 20);
}

TEST_MAIN("median_filter")
    RUN(test_init_valid);
    RUN(test_init_invalid);
    RUN(test_window_3);
    RUN(test_spike_rejection);
    RUN(test_constant_signal);
    RUN(test_descending);
    RUN(test_negative_values);
    RUN(test_reset);
    RUN(test_get);
TEST_MAIN_END()
