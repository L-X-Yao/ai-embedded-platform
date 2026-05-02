// SPDX-License-Identifier: MIT
/*
 * mag_cal — min/max magnetometer calibration (hard-iron + scale)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef MAG_CAL_H
#define MAG_CAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- configuration ---------- */

#ifndef MAG_CAL_MAX_SAMPLES
#define MAG_CAL_MAX_SAMPLES 500
#endif

#ifndef MAG_CAL_MIN_SPREAD
#define MAG_CAL_MIN_SPREAD 0.005f
#endif

/* ---------- result ---------- */

typedef struct {
    float offset_x, offset_y, offset_z;
    float scale_x, scale_y, scale_z;
} mag_cal_result_t;

/* ---------- state ---------- */

typedef enum {
    MAG_CAL_IDLE = 0,
    MAG_CAL_COLLECTING,
    MAG_CAL_DONE_OK,
    MAG_CAL_DONE_FAIL,
} mag_cal_state_t;

typedef struct {
    float samples[MAG_CAL_MAX_SAMPLES][3];
    uint16_t count;
    mag_cal_state_t state;
    mag_cal_result_t result;
} mag_cal_t;

/* ---------- API ---------- */

/** Initialize calibration state. @thread_safe: no @isr_safe: no */
void mag_cal_init(mag_cal_t *cal);

/** Start collecting samples. @thread_safe: caller_must_lock @isr_safe: yes */
void mag_cal_start(mag_cal_t *cal);

/** Abort calibration and return to IDLE. @thread_safe: caller_must_lock @isr_safe: yes */
void mag_cal_abort(mag_cal_t *cal);

/** Feed one magnetometer sample. @thread_safe: caller_must_lock @isr_safe: yes */
bool mag_cal_feed(mag_cal_t *cal, float mx, float my, float mz);

/** Compute calibration from collected samples. @thread_safe: caller_must_lock @isr_safe: yes */
mag_cal_state_t mag_cal_compute(mag_cal_t *cal);

/** Get current state. @thread_safe: caller_must_lock @isr_safe: yes */
mag_cal_state_t mag_cal_get_state(const mag_cal_t *cal);

/** Get result (valid only when state == DONE_OK). @thread_safe: caller_must_lock @isr_safe: yes */
const mag_cal_result_t *mag_cal_get_result(const mag_cal_t *cal);

/** Apply calibration to raw reading. @thread_safe: yes (pure) @isr_safe: yes */
void mag_cal_apply(const mag_cal_result_t *cal,
                   float raw_x, float raw_y, float raw_z,
                   float *out_x, float *out_y, float *out_z);

/** Get number of collected samples. @thread_safe: caller_must_lock @isr_safe: yes */
uint16_t mag_cal_sample_count(const mag_cal_t *cal);

#ifdef __cplusplus
}
#endif

#endif /* MAG_CAL_H */
