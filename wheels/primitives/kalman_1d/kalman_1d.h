// SPDX-License-Identifier: MIT
/* kalman_1d — 1D scalar Kalman filter
 * Layer: L0  Status: candidate  Deps: (none)  Instances: multi */
#ifndef KALMAN_1D_H
#define KALMAN_1D_H

#ifdef __cplusplus
extern "C" {
#endif

#define KALMAN_1D_OK         0
#define KALMAN_1D_ERR_PARAM -1

typedef struct {
    float q;    /* process noise covariance (> 0) */
    float r;    /* measurement noise covariance (> 0) */
} kalman_1d_cfg_t;

typedef struct {
    float x;    /* state estimate */
    float p;    /* estimate error covariance */
    float q;    /* process noise covariance */
    float r;    /* measurement noise covariance */
    float k;    /* last Kalman gain */
} kalman_1d_t;

/* @thread_safe: no  @isr_safe: no */
int kalman_1d_init(kalman_1d_t *kf, const kalman_1d_cfg_t *cfg);

/* Feed measurement, return updated state estimate.
 * First call sets x=measurement, P=1.0 (bootstrap).
 * Returns 0.0f on NULL; returns current estimate unchanged if z is NaN.
 * @thread_safe: no  @isr_safe: yes (if instance not shared) */
float kalman_1d_update(kalman_1d_t *kf, float z);

/* @thread_safe: yes  @isr_safe: yes */
float kalman_1d_get(const kalman_1d_t *kf);

/* @thread_safe: yes  @isr_safe: yes */
float kalman_1d_get_gain(const kalman_1d_t *kf);

/* @thread_safe: no  @isr_safe: no */
void kalman_1d_reset(kalman_1d_t *kf);

/* Change noise parameters at runtime. Both must be > 0.
 * @thread_safe: no  @isr_safe: no */
int kalman_1d_set_noise(kalman_1d_t *kf, float q, float r);

#ifdef __cplusplus
}
#endif
#endif /* KALMAN_1D_H */
