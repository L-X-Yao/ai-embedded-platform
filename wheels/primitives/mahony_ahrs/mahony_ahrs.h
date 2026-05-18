// SPDX-License-Identifier: MIT
/*
 * mahony_ahrs — Mahony AHRS complementary filter (6/9-axis)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef MAHONY_AHRS_H
#define MAHONY_AHRS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- configuration ---------- */

typedef struct {
    float kp;
    float ki;
    float sample_freq_hz;
} mahony_ahrs_cfg_t;

/* ---------- filter state ---------- */

typedef struct {
    float q0, q1, q2, q3;
    float integral_fb_x, integral_fb_y, integral_fb_z;
    float half_t;
    float kp;
    float ki;
    float inv_freq;
} mahony_ahrs_t;

/* ---------- output ---------- */

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
} mahony_ahrs_euler_t;

/* ---------- API ---------- */

/** Return sensible default config (Kp=2.0, Ki=0.005, 200 Hz). @thread_safe: yes (pure) @isr_safe: yes */
mahony_ahrs_cfg_t mahony_ahrs_default_cfg(void);

/** Initialize filter state. @thread_safe: no @isr_safe: no */
void mahony_ahrs_init(mahony_ahrs_t *f, const mahony_ahrs_cfg_t *cfg);

/** Update with 9-axis data. Pass mx=my=mz=0 for 6-axis. @thread_safe: caller_must_lock @isr_safe: no */
void mahony_ahrs_update(mahony_ahrs_t *f,
                   float gx_dps, float gy_dps, float gz_dps,
                   float ax, float ay, float az,
                   float mx, float my, float mz);

/** Extract Euler angles from current quaternion. @thread_safe: caller_must_lock @isr_safe: yes */
void mahony_ahrs_get_euler(const mahony_ahrs_t *f, mahony_ahrs_euler_t *out);

/** Get quaternion directly (q0=w, q1=x, q2=y, q3=z). @thread_safe: caller_must_lock @isr_safe: yes */
void mahony_ahrs_get_quaternion(const mahony_ahrs_t *f, float *q0, float *q1, float *q2, float *q3);

#ifdef __cplusplus
}
#endif

#endif /* MAHONY_AHRS_H */
