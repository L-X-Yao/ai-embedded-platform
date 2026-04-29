#include "median_filter.h"
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

int main(void)
{
    printf("median_filter tests:\n");
    RUN(test_init_valid);
    RUN(test_init_invalid);
    RUN(test_window_3);
    RUN(test_spike_rejection);
    RUN(test_constant_signal);
    RUN(test_descending);
    RUN(test_negative_values);
    RUN(test_reset);
    RUN(test_get);
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
