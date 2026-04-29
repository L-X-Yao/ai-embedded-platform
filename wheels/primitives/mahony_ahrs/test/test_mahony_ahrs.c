#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../mahony_ahrs.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((float)(a)-(float)(b)) < (tol), msg)

static void test_default_cfg(void)
{
    printf("test_default_cfg\n");
    mahony_cfg_t cfg = mahony_default_cfg();
    ASSERT_NEAR(cfg.kp, 2.0f, 0.01f, "kp=2.0");
    ASSERT_NEAR(cfg.ki, 0.005f, 0.001f, "ki=0.005");
    ASSERT_NEAR(cfg.sample_freq_hz, 200.0f, 0.1f, "freq=200");
}

static void test_init_quaternion_identity(void)
{
    printf("test_init_quaternion_identity\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    float q0, q1, q2, q3;
    mahony_get_quaternion(&f, &q0, &q1, &q2, &q3);
    ASSERT_NEAR(q0, 1.0f, 0.001f, "q0=1");
    ASSERT_NEAR(q1, 0.0f, 0.001f, "q1=0");
    ASSERT_NEAR(q2, 0.0f, 0.001f, "q2=0");
    ASSERT_NEAR(q3, 0.0f, 0.001f, "q3=0");
}

static void test_init_euler_zero(void)
{
    printf("test_init_euler_zero\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 0.1f, "roll=0");
    ASSERT_NEAR(e.pitch_deg, 0.0f, 0.1f, "pitch=0");
    ASSERT_NEAR(e.yaw_deg, 0.0f, 0.1f, "yaw=0");
}

static void test_level_accel_converges(void)
{
    printf("test_level_accel_converges\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    for (int i = 0; i < 2000; i++) {
        mahony_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 1.0f, "roll near 0");
    ASSERT_NEAR(e.pitch_deg, 0.0f, 1.0f, "pitch near 0");
}

static void test_tilted_accel(void)
{
    printf("test_tilted_accel\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    float ax = sinf(30.0f * 3.14159265f / 180.0f);
    float az = cosf(30.0f * 3.14159265f / 180.0f);

    for (int i = 0; i < 4000; i++) {
        mahony_update(&f, 0, 0, 0, ax, 0, az, 0, 0, 0);
    }

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.pitch_deg, -30.0f, 3.0f, "pitch ~-30 for forward tilt");
}

static void test_gyro_integration(void)
{
    printf("test_gyro_integration\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.kp = 0.0f;
    cfg.ki = 0.0f;
    mahony_init(&f, &cfg);

    for (int i = 0; i < 200; i++) {
        mahony_update(&f, 0, 0, 90.0f, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.yaw_deg, 90.0f, 5.0f, "yaw ~90 after 1s at 90dps");
}

static void test_null_safety(void)
{
    printf("test_null_safety\n");
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_euler_t e;
    float q;

    mahony_init(NULL, &cfg);
    mahony_init(NULL, NULL);
    mahony_update(NULL, 0, 0, 0, 0, 0, 1, 0, 0, 0);
    mahony_get_euler(NULL, &e);
    mahony_get_euler(NULL, NULL);
    mahony_get_quaternion(NULL, &q, &q, &q, &q);
    ASSERT(1, "null calls safe");

    mahony_t f;
    mahony_init(&f, &cfg);
    mahony_get_quaternion(&f, NULL, NULL, NULL, NULL);
    ASSERT(1, "null output ptrs safe");
}

static void test_reinit_resets(void)
{
    printf("test_reinit_resets\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_update(&f, 10, 20, 30, 0, 0, 1, 0, 0, 0);
    }

    mahony_init(&f, &cfg);
    float q0, q1, q2, q3;
    mahony_get_quaternion(&f, &q0, &q1, &q2, &q3);
    ASSERT_NEAR(q0, 1.0f, 0.001f, "q0 reset to 1");
    ASSERT_NEAR(q1, 0.0f, 0.001f, "q1 reset to 0");
    ASSERT_NEAR(f.integral_fb_x, 0.0f, 0.001f, "integral reset");
}

static void test_quaternion_norm(void)
{
    printf("test_quaternion_norm\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    for (int i = 0; i < 5000; i++) {
        mahony_update(&f, 5, -3, 15, 0.1f, -0.2f, 0.97f, 0.3f, -0.1f, 0.5f);
    }

    float q0, q1, q2, q3;
    mahony_get_quaternion(&f, &q0, &q1, &q2, &q3);
    float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    ASSERT_NEAR(norm, 1.0f, 0.001f, "quaternion stays unit length");
}

static void test_zero_accel_skipped(void)
{
    printf("test_zero_accel_skipped\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    mahony_update(&f, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    float q0;
    mahony_get_quaternion(&f, &q0, NULL, NULL, NULL);
    ASSERT_NEAR(q0, 1.0f, 0.001f, "identity preserved when accel=0");
}

static void test_6axis_vs_9axis(void)
{
    printf("test_6axis_vs_9axis\n");
    mahony_t f6, f9;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f6, &cfg);
    mahony_init(&f9, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_update(&f6, 0, 0, 0, 0, 0, 1, 0, 0, 0);
        mahony_update(&f9, 0, 0, 0, 0, 0, 1, 0.3f, 0, 0.5f);
    }

    mahony_euler_t e6, e9;
    mahony_get_euler(&f6, &e6);
    mahony_get_euler(&f9, &e9);
    ASSERT_NEAR(e6.roll_deg, 0.0f, 1.0f, "6-axis roll~0");
    ASSERT_NEAR(e6.pitch_deg, 0.0f, 1.0f, "6-axis pitch~0");
    ASSERT_NEAR(e9.roll_deg, 0.0f, 1.0f, "9-axis roll~0");
    ASSERT_NEAR(e9.pitch_deg, 0.0f, 1.0f, "9-axis pitch~0");
}

static void test_zero_freq_guard(void)
{
    printf("test_zero_freq_guard\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.sample_freq_hz = 0.0f;
    mahony_init(&f, &cfg);
    mahony_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT(!isnan(e.roll_deg), "roll not NaN with freq=0");
    ASSERT(!isnan(e.pitch_deg), "pitch not NaN with freq=0");
    ASSERT(!isnan(e.yaw_deg), "yaw not NaN with freq=0");
}

static void test_negative_freq_guard(void)
{
    printf("test_negative_freq_guard\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.sample_freq_hz = -100.0f;
    mahony_init(&f, &cfg);
    mahony_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT(!isnan(e.roll_deg), "roll not NaN with negative freq");
}

static void test_gimbal_lock_pitch90(void)
{
    printf("test_gimbal_lock_pitch90\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    mahony_init(&f, &cfg);

    for (int i = 0; i < 4000; i++) {
        mahony_update(&f, 0, 0, 0, 1.0f, 0, 0, 0, 0, 0);
    }

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.pitch_deg, -90.0f, 3.0f, "pitch near -90 (gimbal lock)");
    ASSERT(!isnan(e.roll_deg), "roll not NaN at gimbal lock");
    ASSERT(!isnan(e.yaw_deg), "yaw not NaN at gimbal lock");
}

static void test_large_gyro_rate_stability(void)
{
    printf("test_large_gyro_rate_stability\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.sample_freq_hz = 1000.0f;
    mahony_init(&f, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_update(&f, 2000, -1500, 1800, 0, 0, 1.0f, 0, 0, 0);
    }

    float q0, q1, q2, q3;
    mahony_get_quaternion(&f, &q0, &q1, &q2, &q3);
    float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    ASSERT_NEAR(norm, 1.0f, 0.01f, "quaternion still unit at high rate");
    ASSERT(!isnan(q0), "q0 not NaN after extreme gyro");
}

static void test_near_inverted_accel(void)
{
    printf("test_near_inverted_accel\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.kp = 5.0f;
    mahony_init(&f, &cfg);

    for (int i = 0; i < 8000; i++) {
        mahony_update(&f, 0, 0, 0, 0.05f, 0, -1.0f, 0, 0, 0);
    }

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT(fabsf(e.roll_deg) > 90.0f || fabsf(e.pitch_deg) > 80.0f,
           "filter tracks near-inverted gravity");
}

static void test_ki_corrects_bias(void)
{
    printf("test_ki_corrects_bias\n");
    mahony_t f;
    mahony_cfg_t cfg = mahony_default_cfg();
    cfg.ki = 0.1f;
    mahony_init(&f, &cfg);

    for (int i = 0; i < 5000; i++) {
        mahony_update(&f, 5.0f, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }

    ASSERT(fabsf(f.integral_fb_x) > 0.001f, "ki accumulates integral");

    mahony_euler_t e;
    mahony_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 5.0f, "ki corrects gyro bias on roll");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_default_cfg();
    test_init_quaternion_identity();
    test_init_euler_zero();
    test_level_accel_converges();
    test_tilted_accel();
    test_gyro_integration();
    test_null_safety();
    test_reinit_resets();
    test_quaternion_norm();
    test_zero_accel_skipped();
    test_6axis_vs_9axis();
    test_zero_freq_guard();
    test_negative_freq_guard();
    test_gimbal_lock_pitch90();
    test_large_gyro_rate_stability();
    test_near_inverted_accel();
    test_ki_corrects_bias();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
