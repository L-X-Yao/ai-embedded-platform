#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../mag_cal.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((float)(a)-(float)(b)) < (tol), msg)

#define PI 3.14159265f

static void generate_sphere(mag_cal_t *cal, float cx, float cy, float cz,
                            float r, int n)
{
    for (int i = 0; i < n; i++) {
        float theta = PI * (float)i / (float)n;
        float phi = 2.0f * PI * (float)(i * 7 % n) / (float)n;
        float x = cx + r * sinf(theta) * cosf(phi);
        float y = cy + r * sinf(theta) * sinf(phi);
        float z = cz + r * cosf(theta);
        mag_cal_feed(cal, x, y, z);
    }
}

static void generate_ellipsoid(mag_cal_t *cal, float cx, float cy, float cz,
                               float rx, float ry, float rz, int n)
{
    for (int i = 0; i < n; i++) {
        float theta = PI * (float)i / (float)n;
        float phi = 2.0f * PI * (float)(i * 7 % n) / (float)n;
        float x = cx + rx * sinf(theta) * cosf(phi);
        float y = cy + ry * sinf(theta) * sinf(phi);
        float z = cz + rz * cosf(theta);
        mag_cal_feed(cal, x, y, z);
    }
}

/* ---------- tests ---------- */

static void test_init(void)
{
    mag_cal_t cal;

    printf("test_init\n");
    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE, "idle after init");
    ASSERT_EQ(mag_cal_sample_count(&cal), 0, "zero samples");
    mag_cal_init(NULL);
    ASSERT(1, "null init safe");
}

static void test_start(void)
{
    mag_cal_t cal;

    printf("test_start\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_COLLECTING, "collecting");
}

static void test_feed_before_start(void)
{
    mag_cal_t cal;

    printf("test_feed_before_start\n");
    mag_cal_init(&cal);
    ASSERT(!mag_cal_feed(&cal, 1, 2, 3), "feed rejected in IDLE");
}

static void test_sphere_calibration(void)
{
    mag_cal_t cal;

    printf("test_sphere_calibration\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0.1f, -0.2f, 0.05f, 0.3f, 500);

    mag_cal_state_t s = mag_cal_compute(&cal);
    ASSERT_EQ(s, MAG_CAL_DONE_OK, "calibration ok");

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT_NEAR(r->offset_x, 0.1f, 0.02f, "offset_x ~0.1");
    ASSERT_NEAR(r->offset_y, -0.2f, 0.02f, "offset_y ~-0.2");
    ASSERT_NEAR(r->offset_z, 0.05f, 0.02f, "offset_z ~0.05");
    ASSERT_NEAR(r->scale_x, 1.0f, 0.1f, "scale_x ~1.0");
    ASSERT_NEAR(r->scale_y, 1.0f, 0.1f, "scale_y ~1.0");
    ASSERT_NEAR(r->scale_z, 1.0f, 0.1f, "scale_z ~1.0");
}

static void test_ellipsoid_calibration(void)
{
    mag_cal_t cal;

    printf("test_ellipsoid_calibration\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_ellipsoid(&cal, 0.0f, 0.0f, 0.0f, 0.2f, 0.4f, 0.3f, 500);

    mag_cal_state_t s = mag_cal_compute(&cal);
    ASSERT_EQ(s, MAG_CAL_DONE_OK, "ellipsoid ok");

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT(r->scale_x > r->scale_y, "X scaled up (smallest radius)");
    ASSERT_NEAR(r->offset_x, 0.0f, 0.02f, "centered X");
    ASSERT_NEAR(r->offset_y, 0.0f, 0.02f, "centered Y");
}

static void test_apply(void)
{
    mag_cal_t cal;

    printf("test_apply\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0.5f, -0.3f, 0.1f, 0.4f, 500);
    mag_cal_compute(&cal);

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    float cx, cy, cz;
    mag_cal_apply(r, 0.5f, -0.3f, 0.1f, &cx, &cy, &cz);
    ASSERT_NEAR(cx, 0.0f, 0.02f, "center → 0 after cal X");
    ASSERT_NEAR(cy, 0.0f, 0.02f, "center → 0 after cal Y");
    ASSERT_NEAR(cz, 0.0f, 0.02f, "center → 0 after cal Z");
}

static void test_insufficient_spread(void)
{
    mag_cal_t cal;

    printf("test_insufficient_spread\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < 500; i++)
        mag_cal_feed(&cal, 0.1f, 0.2f, 0.3f);

    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL, "constant samples fail");
}

static void test_zero_samples_fail(void)
{
    mag_cal_t cal;

    printf("test_zero_samples_fail\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL, "zero samples fail");
}

static void test_null_safety(void)
{
    printf("test_null_safety\n");
    ASSERT_EQ(mag_cal_get_state(NULL), MAG_CAL_IDLE, "null state");
    ASSERT(mag_cal_get_result(NULL) == NULL, "null result");
    ASSERT(!mag_cal_feed(NULL, 0, 0, 0), "null feed");
    ASSERT_EQ(mag_cal_sample_count(NULL), 0, "null count");
    mag_cal_start(NULL);
    float x, y, z;
    mag_cal_apply(NULL, 1, 2, 3, &x, &y, &z);
    ASSERT(1, "null apply safe");
}

static void test_reinit_clears(void)
{
    mag_cal_t cal;

    printf("test_reinit_clears\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0, 0, 0, 1.0f, 100);
    ASSERT_EQ(mag_cal_sample_count(&cal), 100, "100 samples");

    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE, "idle after reinit");
    ASSERT_EQ(mag_cal_sample_count(&cal), 0, "zero samples after reinit");
}

static void test_buffer_full(void)
{
    mag_cal_t cal;

    printf("test_buffer_full\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < MAG_CAL_MAX_SAMPLES; i++)
        ASSERT(mag_cal_feed(&cal, (float)i, 0, 0), "feed ok");
    ASSERT(!mag_cal_feed(&cal, 999, 0, 0), "buffer full rejects");
    ASSERT_EQ(mag_cal_sample_count(&cal), MAG_CAL_MAX_SAMPLES, "max samples");
}

static void test_single_sample(void)
{
    mag_cal_t cal;

    printf("test_single_sample\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    mag_cal_feed(&cal, 1.0f, 2.0f, 3.0f);
    ASSERT_EQ(mag_cal_sample_count(&cal), 1, "1 sample");
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL,
              "single sample insufficient spread");
}

static void test_collinear_samples(void)
{
    mag_cal_t cal;

    printf("test_collinear_samples\n");
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < 500; i++)
        mag_cal_feed(&cal, (float)i * 0.01f, 0.0f, 0.0f);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL,
              "collinear samples (Y/Z zero spread) fail");
}

static void test_apply_null_outputs(void)
{
    mag_cal_result_t r = { .offset_x = 1, .offset_y = 2, .offset_z = 3,
                           .scale_x = 1, .scale_y = 1, .scale_z = 1 };
    float x = 999.0f, z = 999.0f;

    printf("test_apply_null_outputs\n");
    mag_cal_apply(&r, 1, 2, 3, &x, NULL, &z);
    ASSERT_NEAR(x, 999.0f, 0.01f, "x untouched when out_y null");
    ASSERT_NEAR(z, 999.0f, 0.01f, "z untouched when out_z null");
}

static void test_compute_before_start(void)
{
    mag_cal_t cal;

    printf("test_compute_before_start\n");
    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL,
              "compute in IDLE fails");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_init();
    test_start();
    test_feed_before_start();
    test_sphere_calibration();
    test_ellipsoid_calibration();
    test_apply();
    test_insufficient_spread();
    test_zero_samples_fail();
    test_null_safety();
    test_reinit_clears();
    test_buffer_full();
    test_single_sample();
    test_collinear_samples();
    test_apply_null_outputs();
    test_compute_before_start();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
