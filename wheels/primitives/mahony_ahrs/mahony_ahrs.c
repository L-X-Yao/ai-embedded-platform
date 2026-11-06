// SPDX-License-Identifier: MIT
/*
 * mahony_ahrs — Mahony AHRS complementary filter (6/9-axis)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "mahony_ahrs.h"
#include "contract.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define DEG_TO_RAD (3.14159265358979f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265358979f)

/* False for NaN as well: every comparison with NaN is false. */
static bool is_finite(float x)
{
    return x >= -FLT_MAX && x <= FLT_MAX;
}

mahony_ahrs_cfg_t mahony_ahrs_default_cfg(void)
{
    mahony_ahrs_cfg_t cfg;
    cfg.kp = 2.0f;
    cfg.ki = 0.005f;
    cfg.sample_freq_hz = 200.0f;
    return cfg;
}

void mahony_ahrs_init(mahony_ahrs_t *f, const mahony_ahrs_cfg_t *cfg)
{
    if (!f || !cfg) return;
    memset(f, 0, sizeof(*f));
    f->q0 = 1.0f;
    f->kp = cfg->kp;
    f->ki = cfg->ki;
    float freq = (cfg->sample_freq_hz > 0.0f) ? cfg->sample_freq_hz : 1.0f;
    f->half_t = 0.5f / freq;
    f->inv_freq = 1.0f / freq;
    ENSURE(f->q0 == 1.0f);
    ENSURE(f->half_t > 0.0f);
}

void mahony_ahrs_update(mahony_ahrs_t *f,
                   float gx_dps, float gy_dps, float gz_dps,
                   float ax, float ay, float az,
                   float mx, float my, float mz)
{
    if (!f) return;

    /* NaN in the state never leaves it, so a bad sample must not get there */
    if (!is_finite(gx_dps) || !is_finite(gy_dps) || !is_finite(gz_dps) ||
        !is_finite(ax) || !is_finite(ay) || !is_finite(az)) return;

    float gx = gx_dps * DEG_TO_RAD;
    float gy = gy_dps * DEG_TO_RAD;
    float gz = gz_dps * DEG_TO_RAD;

    /* A zero vector is no gravity reference: like the reference filter,
     * skip all the feedback and still integrate the gyro. */
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 0.0f) {
        float vx, vy, vz;
        float ex, ey, ez;

        ax /= norm; ay /= norm; az /= norm;

        vx = 2.0f * (f->q1 * f->q3 - f->q0 * f->q2);
        vy = 2.0f * (f->q0 * f->q1 + f->q2 * f->q3);
        vz = f->q0 * f->q0 - f->q1 * f->q1 - f->q2 * f->q2 + f->q3 * f->q3;

        ex = (ay * vz - az * vy);
        ey = (az * vx - ax * vz);
        ez = (ax * vy - ay * vx);

        /* The magnetometer is optional: a bad sample counts as not available */
        float mag_norm = sqrtf(mx * mx + my * my + mz * mz);
        if (is_finite(mag_norm) && mag_norm > 0.001f) {
            mx /= mag_norm; my /= mag_norm; mz /= mag_norm;

            float hx = 2.0f * (mx * (0.5f - f->q2*f->q2 - f->q3*f->q3) +
                                my * (f->q1*f->q2 - f->q0*f->q3) +
                                mz * (f->q1*f->q3 + f->q0*f->q2));
            float hy = 2.0f * (mx * (f->q1*f->q2 + f->q0*f->q3) +
                                my * (0.5f - f->q1*f->q1 - f->q3*f->q3) +
                                mz * (f->q2*f->q3 - f->q0*f->q1));
            float bx = sqrtf(hx * hx + hy * hy);
            float bz = 2.0f * (mx * (f->q1*f->q3 - f->q0*f->q2) +
                                my * (f->q2*f->q3 + f->q0*f->q1) +
                                mz * (0.5f - f->q1*f->q1 - f->q2*f->q2));

            float wx = bx * (0.5f - f->q2*f->q2 - f->q3*f->q3) + bz * (f->q1*f->q3 - f->q0*f->q2);
            float wy = bx * (f->q1*f->q2 - f->q0*f->q3)         + bz * (f->q0*f->q1 + f->q2*f->q3);
            float wz = bx * (f->q0*f->q2 + f->q1*f->q3)         + bz * (0.5f - f->q1*f->q1 - f->q2*f->q2);

            ex += (my * wz - mz * wy);
            ey += (mz * wx - mx * wz);
            ez += (mx * wy - my * wx);
        }

        f->integral_fb_x += ex * f->ki * f->inv_freq;
        f->integral_fb_y += ey * f->ki * f->inv_freq;
        f->integral_fb_z += ez * f->ki * f->inv_freq;

        #define MAHONY_INTEGRAL_MAX 1.0f
        if (f->integral_fb_x >  MAHONY_INTEGRAL_MAX) f->integral_fb_x =  MAHONY_INTEGRAL_MAX;
        if (f->integral_fb_x < -MAHONY_INTEGRAL_MAX) f->integral_fb_x = -MAHONY_INTEGRAL_MAX;
        if (f->integral_fb_y >  MAHONY_INTEGRAL_MAX) f->integral_fb_y =  MAHONY_INTEGRAL_MAX;
        if (f->integral_fb_y < -MAHONY_INTEGRAL_MAX) f->integral_fb_y = -MAHONY_INTEGRAL_MAX;
        if (f->integral_fb_z >  MAHONY_INTEGRAL_MAX) f->integral_fb_z =  MAHONY_INTEGRAL_MAX;
        if (f->integral_fb_z < -MAHONY_INTEGRAL_MAX) f->integral_fb_z = -MAHONY_INTEGRAL_MAX;

        gx += f->kp * ex;
        gy += f->kp * ey;
        gz += f->kp * ez;
        gx += f->integral_fb_x;
        gy += f->integral_fb_y;
        gz += f->integral_fb_z;
    }

    float nq0 = f->q0 + (-f->q1 * gx - f->q2 * gy - f->q3 * gz) * f->half_t;
    float nq1 = f->q1 + ( f->q0 * gx + f->q2 * gz - f->q3 * gy) * f->half_t;
    float nq2 = f->q2 + ( f->q0 * gy - f->q1 * gz + f->q3 * gx) * f->half_t;
    float nq3 = f->q3 + ( f->q0 * gz + f->q1 * gy - f->q2 * gx) * f->half_t;

    norm = sqrtf(nq0 * nq0 + nq1 * nq1 + nq2 * nq2 + nq3 * nq3);
    if (!is_finite(norm) || norm < 1e-8f) {
        /* Clear the integrators too: they feed every later step */
        f->q0 = 1.0f; f->q1 = f->q2 = f->q3 = 0.0f;
        f->integral_fb_x = f->integral_fb_y = f->integral_fb_z = 0.0f;
        return;
    }
    f->q0 = nq0 / norm;
    f->q1 = nq1 / norm;
    f->q2 = nq2 / norm;
    f->q3 = nq3 / norm;
}

void mahony_ahrs_get_euler(const mahony_ahrs_t *f, mahony_ahrs_euler_t *out)
{
    if (!f || !out) return;

    out->roll_deg = atan2f(2.0f * (f->q0 * f->q1 + f->q2 * f->q3),
                           1.0f - 2.0f * (f->q1 * f->q1 + f->q2 * f->q2)) * RAD_TO_DEG;

    float sinp = 2.0f * (f->q0 * f->q2 - f->q3 * f->q1);
    if (fabsf(sinp) >= 1.0f)
        out->pitch_deg = copysignf(90.0f, sinp);
    else
        out->pitch_deg = asinf(sinp) * RAD_TO_DEG;

    out->yaw_deg = atan2f(2.0f * (f->q0 * f->q3 + f->q1 * f->q2),
                          1.0f - 2.0f * (f->q2 * f->q2 + f->q3 * f->q3)) * RAD_TO_DEG;
}

void mahony_ahrs_get_quaternion(const mahony_ahrs_t *f, float *q0, float *q1, float *q2, float *q3)
{
    if (!f) return;
    if (q0) *q0 = f->q0;
    if (q1) *q1 = f->q1;
    if (q2) *q2 = f->q2;
    if (q3) *q3 = f->q3;
}
