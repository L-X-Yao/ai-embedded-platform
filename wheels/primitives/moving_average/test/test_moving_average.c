#include <stdio.h>
#include <math.h>
#include "../moving_average.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a) - (b)) < (tol), msg)

/* === SMA tests === */

static void test_sma_init(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[4];
    ASSERT_EQ(moving_average_sma_init(&sma, buf, 4), MOVING_AVERAGE_OK, "init ok");
    ASSERT_EQ(sma.count, 0, "count zero");
    ASSERT(!moving_average_sma_is_full(&sma), "not full initially");
}

static void test_sma_null_safety(void)
{
    printf("%s\n", __func__);
    float buf[4];
    ASSERT_EQ(moving_average_sma_init(NULL, buf, 4), MOVING_AVERAGE_ERR_PARAM, "null ctx");
    moving_average_sma_t sma;
    ASSERT_EQ(moving_average_sma_init(&sma, NULL, 4), MOVING_AVERAGE_ERR_PARAM, "null buf");
    ASSERT_EQ(moving_average_sma_init(&sma, buf, 0), MOVING_AVERAGE_ERR_PARAM, "zero window");
    ASSERT_EQ(moving_average_sma_reset(NULL), MOVING_AVERAGE_ERR_PARAM, "reset null");
    ASSERT_NEAR(moving_average_sma_feed(NULL, 1.0f), 0.0f, 1e-6f, "feed null");
    ASSERT_NEAR(moving_average_sma_get(NULL), 0.0f, 1e-6f, "get null");
    ASSERT(!moving_average_sma_is_full(NULL), "is_full null");
}

static void test_sma_single_sample(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    float avg = moving_average_sma_feed(&sma, 10.0f);
    ASSERT_NEAR(avg, 10.0f, 1e-6f, "single sample = sample");
    ASSERT_NEAR(moving_average_sma_get(&sma), 10.0f, 1e-6f, "get matches feed return");
}

static void test_sma_filling(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);

    moving_average_sma_feed(&sma, 2.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 2.0f, 1e-6f, "1 sample");

    moving_average_sma_feed(&sma, 4.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 3.0f, 1e-6f, "2 samples avg=3");

    moving_average_sma_feed(&sma, 6.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 4.0f, 1e-6f, "3 samples avg=4");

    ASSERT(!moving_average_sma_is_full(&sma), "not full at 3/4");

    moving_average_sma_feed(&sma, 8.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 5.0f, 1e-6f, "4 samples avg=5");
    ASSERT(moving_average_sma_is_full(&sma), "full at 4/4");
}

static void test_sma_sliding(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[3];
    moving_average_sma_init(&sma, buf, 3);

    moving_average_sma_feed(&sma, 1.0f);
    moving_average_sma_feed(&sma, 2.0f);
    moving_average_sma_feed(&sma, 3.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 2.0f, 1e-6f, "window [1,2,3] avg=2");

    moving_average_sma_feed(&sma, 6.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), (2.0f + 3.0f + 6.0f) / 3.0f, 1e-4f,
                "window [2,3,6] avg=3.667");

    moving_average_sma_feed(&sma, 9.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), (3.0f + 6.0f + 9.0f) / 3.0f, 1e-4f,
                "window [3,6,9] avg=6");
}

static void test_sma_reset(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    moving_average_sma_feed(&sma, 100.0f);
    moving_average_sma_feed(&sma, 200.0f);

    moving_average_sma_reset(&sma);
    ASSERT_EQ(sma.count, 0, "count cleared");
    ASSERT_NEAR(sma.sum, 0.0f, 1e-6f, "sum cleared");
    ASSERT(!moving_average_sma_is_full(&sma), "not full after reset");

    float avg = moving_average_sma_feed(&sma, 5.0f);
    ASSERT_NEAR(avg, 5.0f, 1e-6f, "fresh after reset");
}

static void test_sma_constant_input(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[8];
    moving_average_sma_init(&sma, buf, 8);
    for (int i = 0; i < 20; i++)
    {
        moving_average_sma_feed(&sma, 42.0f);
    }
    ASSERT_NEAR(moving_average_sma_get(&sma), 42.0f, 1e-4f, "constant input => constant output");
}

static void test_sma_window_1(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[1];
    moving_average_sma_init(&sma, buf, 1);
    ASSERT_NEAR(moving_average_sma_feed(&sma, 5.0f), 5.0f, 1e-6f, "win=1 first");
    ASSERT_NEAR(moving_average_sma_feed(&sma, 10.0f), 10.0f, 1e-6f, "win=1 second");
}

static void test_sma_get_empty(void)
{
    printf("%s\n", __func__);
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    ASSERT_NEAR(moving_average_sma_get(&sma), 0.0f, 1e-6f, "empty returns 0");
}

/* === EMA tests === */

static void test_ema_init(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    ASSERT_EQ(moving_average_ema_init(&ema, 0.5f), MOVING_AVERAGE_OK, "init ok");
    ASSERT(!ema.primed, "not primed");
}

static void test_ema_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(moving_average_ema_init(NULL, 0.5f), MOVING_AVERAGE_ERR_PARAM, "null ctx");
    moving_average_ema_t ema;
    ASSERT_EQ(moving_average_ema_init(&ema, 0.0f), MOVING_AVERAGE_ERR_PARAM, "alpha=0");
    ASSERT_EQ(moving_average_ema_init(&ema, -0.1f), MOVING_AVERAGE_ERR_PARAM, "alpha<0");
    ASSERT_EQ(moving_average_ema_init(&ema, 1.1f), MOVING_AVERAGE_ERR_PARAM, "alpha>1");
    ASSERT_EQ(moving_average_ema_reset(NULL), MOVING_AVERAGE_ERR_PARAM, "reset null");
    ASSERT_NEAR(moving_average_ema_feed(NULL, 1.0f), 0.0f, 1e-6f, "feed null");
    ASSERT_NEAR(moving_average_ema_get(NULL), 0.0f, 1e-6f, "get null");
}

static void test_ema_first_sample(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.3f);
    float val = moving_average_ema_feed(&ema, 100.0f);
    ASSERT_NEAR(val, 100.0f, 1e-6f, "first sample = sample");
    ASSERT(ema.primed, "primed after first feed");
}

static void test_ema_smoothing(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    moving_average_ema_feed(&ema, 0.0f);
    float val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 5.0f, 1e-6f, "alpha=0.5: (0+10)/2 = 5");

    val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 7.5f, 1e-4f, "alpha=0.5: 0.5*10 + 0.5*5 = 7.5");
}

static void test_ema_alpha_1(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 1.0f);
    moving_average_ema_feed(&ema, 5.0f);
    float val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 10.0f, 1e-6f, "alpha=1.0: no smoothing");
}

static void test_ema_convergence(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.1f);
    moving_average_ema_feed(&ema, 0.0f);
    for (int i = 0; i < 100; i++)
    {
        moving_average_ema_feed(&ema, 50.0f);
    }
    ASSERT_NEAR(moving_average_ema_get(&ema), 50.0f, 0.01f, "converges to constant input");
}

static void test_ema_reset(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    moving_average_ema_feed(&ema, 100.0f);
    moving_average_ema_reset(&ema);
    ASSERT(!ema.primed, "not primed after reset");
    float val = moving_average_ema_feed(&ema, 7.0f);
    ASSERT_NEAR(val, 7.0f, 1e-6f, "fresh after reset");
}

static void test_ema_get(void)
{
    printf("%s\n", __func__);
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    ASSERT_NEAR(moving_average_ema_get(&ema), 0.0f, 1e-6f, "get before feed");
    moving_average_ema_feed(&ema, 42.0f);
    ASSERT_NEAR(moving_average_ema_get(&ema), 42.0f, 1e-6f, "get after feed");
}

int main(void)
{
    printf("test_moving_average\n");

    test_sma_init();
    test_sma_null_safety();
    test_sma_single_sample();
    test_sma_filling();
    test_sma_sliding();
    test_sma_reset();
    test_sma_constant_input();
    test_sma_window_1();
    test_sma_get_empty();

    test_ema_init();
    test_ema_null_safety();
    test_ema_first_sample();
    test_ema_smoothing();
    test_ema_alpha_1();
    test_ema_convergence();
    test_ema_reset();
    test_ema_get();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
