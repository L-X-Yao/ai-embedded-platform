#include "timeout.h"
#include <stdio.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) static void name(void)
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL %s:%d: %u != %u\n", __func__, __LINE__, \
               (unsigned)(a), (unsigned)(b)); \
        g_fail++; return; \
    } } while(0)
#define ASSERT_TRUE(x)  ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x) ASSERT_EQ(!!(x), 0)
#define RUN(fn) do { int pf = g_fail; fn(); if(g_fail == pf) { \
    printf("  PASS %s\n", #fn); g_pass++; } } while(0)

TEST(test_basic_expiry)
{
    timeout_t t;
    timeout_set(&t, 1000, 500);
    ASSERT_FALSE(timeout_expired(&t, 1000));
    ASSERT_FALSE(timeout_expired(&t, 1400));
    ASSERT_TRUE(timeout_expired(&t, 1500));
    ASSERT_TRUE(timeout_expired(&t, 2000));
}

TEST(test_remaining)
{
    timeout_t t;
    timeout_set(&t, 100, 1000);
    ASSERT_EQ(timeout_remaining(&t, 100), 1000);
    ASSERT_EQ(timeout_remaining(&t, 600), 500);
    ASSERT_EQ(timeout_remaining(&t, 1100), 0);
    ASSERT_EQ(timeout_remaining(&t, 2000), 0);
}

TEST(test_elapsed)
{
    timeout_t t;
    timeout_set(&t, 500, 1000);
    ASSERT_EQ(timeout_elapsed(&t, 500), 0);
    ASSERT_EQ(timeout_elapsed(&t, 750), 250);
    ASSERT_EQ(timeout_elapsed(&t, 1500), 1000);
}

TEST(test_wraparound)
{
    timeout_t t;
    timeout_set(&t, 0xFFFFFFF0u, 32);
    ASSERT_FALSE(timeout_expired(&t, 0xFFFFFFF0u));
    ASSERT_FALSE(timeout_expired(&t, 0x00000000u));
    ASSERT_TRUE(timeout_expired(&t, 0x00000010u));
    ASSERT_EQ(timeout_remaining(&t, 0x00000000u), 16);
    ASSERT_EQ(timeout_elapsed(&t, 0x00000005u), 21);
}

TEST(test_restart)
{
    timeout_t t;
    timeout_set(&t, 100, 500);
    ASSERT_TRUE(timeout_expired(&t, 700));
    timeout_restart(&t, 700);
    ASSERT_FALSE(timeout_expired(&t, 700));
    ASSERT_FALSE(timeout_expired(&t, 1100));
    ASSERT_TRUE(timeout_expired(&t, 1200));
}

TEST(test_extend)
{
    timeout_t t;
    timeout_set(&t, 0, 100);
    ASSERT_TRUE(timeout_expired(&t, 100));
    timeout_extend(&t, 50);
    ASSERT_FALSE(timeout_expired(&t, 100));
    ASSERT_TRUE(timeout_expired(&t, 150));
}

TEST(test_zero_duration)
{
    timeout_t t;
    timeout_set(&t, 1000, 0);
    ASSERT_TRUE(timeout_expired(&t, 1000));
    ASSERT_TRUE(timeout_expired(&t, 999));
}

TEST(test_max_duration)
{
    timeout_t t;
    timeout_set(&t, 0, 0xFFFFFFFFu);
    ASSERT_FALSE(timeout_expired(&t, 0));
    ASSERT_FALSE(timeout_expired(&t, 0x7FFFFFFFu));
    ASSERT_TRUE(timeout_expired(&t, 0xFFFFFFFFu));
}

TEST(test_null_safety)
{
    timeout_set(NULL, 0, 100);
    ASSERT_TRUE(timeout_expired(NULL, 0));
    ASSERT_EQ(timeout_remaining(NULL, 0), 0);
    ASSERT_EQ(timeout_elapsed(NULL, 0), 0);
    timeout_restart(NULL, 0);
    timeout_extend(NULL, 100);
}

int main(void)
{
    printf("timeout tests:\n");
    RUN(test_basic_expiry);
    RUN(test_remaining);
    RUN(test_elapsed);
    RUN(test_wraparound);
    RUN(test_restart);
    RUN(test_extend);
    RUN(test_zero_duration);
    RUN(test_max_duration);
    RUN(test_null_safety);
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
