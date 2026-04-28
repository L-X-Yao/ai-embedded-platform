/*
 * mahony_ahrs — Mahony AHRS complementary filter (6/9-axis)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "mahony_ahrs.h"
#include <math.h>
#include <string.h>

#define DEG_TO_RAD (3.14159265358979f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265358979f)

mahony_cfg_t mahony_default_cfg(void)
{
    mahony_cfg_t cfg;
    cfg.kp = 2.0f;
    cfg.ki = 0.005f;
    cfg.sample_freq_hz = 200.0f;
    return cfg;
}

void mahony_init(mahony_t *f, const mahony_cfg_t *cfg)
{
    if (!f || !cfg) return;
    memset(f, 0, sizeof(*f));
    f->q0 = 1.0f;
    f->kp = cfg->kp;
    f->ki = cfg->ki;
    f->half_t = 0.5f / cfg->sample_freq_hz;
    f->inv_freq = 1.0f / cfg->sample_freq_hz;
}

void mahony_update(mahony_t *f,
                   float gx_dps, float gy_dps, float gz_dps,
                   float ax, float ay, float az,
                   float mx, float my, float mz)
{
    if (!f) return;

    float norm;
    float vx, vy, vz;
    float ex, ey, ez;

    norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm <= 0.0f) return;
    ax /= norm; ay /= norm; az /= norm;

    vx = 2.0f * (f->q1 * f->q3 - f->q0 * f->q2);
    vy = 2.0f * (f->q0 * f->q1 + f->q2 * f->q3);
    vz = f->q0 * f->q0 - f->q1 * f->q1 - f->q2 * f->q2 + f->q3 * f->q3;

    ex = (ay * vz - az * vy);
    ey = (az * vx - ax * vz);
    ez = (ax * vy - ay * vx);

    float mag_norm = sqrtf(mx * mx + my * my + mz * mz);
    if (mag_norm > 0.001f) {
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

    float gx = gx_dps * DEG_TO_RAD + f->kp * ex + f->integral_fb_x;
    float gy = gy_dps * DEG_TO_RAD + f->kp * ey + f->integral_fb_y;
    float gz = gz_dps * DEG_TO_RAD + f->kp * ez + f->integral_fb_z;

    float nq0 = f->q0 + (-f->q1 * gx - f->q2 * gy - f->q3 * gz) * f->half_t;
    float nq1 = f->q1 + ( f->q0 * gx + f->q2 * gz - f->q3 * gy) * f->half_t;
    float nq2 = f->q2 + ( f->q0 * gy - f->q1 * gz + f->q3 * gx) * f->half_t;
    float nq3 = f->q3 + ( f->q0 * gz + f->q1 * gy - f->q2 * gx) * f->half_t;

    norm = sqrtf(nq0 * nq0 + nq1 * nq1 + nq2 * nq2 + nq3 * nq3);
    f->q0 = nq0 / norm;
    f->q1 = nq1 / norm;
    f->q2 = nq2 / norm;
    f->q3 = nq3 / norm;
}

void mahony_get_euler(const mahony_t *f, mahony_euler_t *out)
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

void mahony_get_quaternion(const mahony_t *f, float *q0, float *q1, float *q2, float *q3)
{
    if (!f) return;
    if (q0) *q0 = f->q0;
    if (q1) *q1 = f->q1;
    if (q2) *q2 = f->q2;
    if (q3) *q3 = f->q3;
}
