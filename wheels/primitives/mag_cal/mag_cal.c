/*
 * mag_cal — min/max magnetometer calibration (hard-iron + scale)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "mag_cal.h"
#include <string.h>

void mag_cal_init(mag_cal_t *cal)
{
    if (!cal) return;
    memset(cal, 0, sizeof(*cal));
    cal->result.scale_x = 1.0f;
    cal->result.scale_y = 1.0f;
    cal->result.scale_z = 1.0f;
}

void mag_cal_start(mag_cal_t *cal)
{
    if (!cal) return;
    cal->count = 0;
    cal->state = MAG_CAL_COLLECTING;
}

bool mag_cal_feed(mag_cal_t *cal, float mx, float my, float mz)
{
    if (!cal || cal->state != MAG_CAL_COLLECTING)
        return false;
    if (cal->count >= MAG_CAL_MAX_SAMPLES)
        return false;

    cal->samples[cal->count][0] = mx;
    cal->samples[cal->count][1] = my;
    cal->samples[cal->count][2] = mz;
    cal->count++;
    return true;
}

mag_cal_state_t mag_cal_compute(mag_cal_t *cal)
{
    if (!cal || cal->count == 0) {
        if (cal) cal->state = MAG_CAL_DONE_FAIL;
        return MAG_CAL_DONE_FAIL;
    }

    float mn[3], mx[3];
    for (int a = 0; a < 3; a++) {
        mn[a] = cal->samples[0][a];
        mx[a] = cal->samples[0][a];
    }

    for (uint16_t i = 1; i < cal->count; i++) {
        for (int a = 0; a < 3; a++) {
            if (cal->samples[i][a] < mn[a]) mn[a] = cal->samples[i][a];
            if (cal->samples[i][a] > mx[a]) mx[a] = cal->samples[i][a];
        }
    }

    float offset[3], radius[3];
    for (int a = 0; a < 3; a++) {
        offset[a] = (mx[a] + mn[a]) * 0.5f;
        radius[a] = (mx[a] - mn[a]) * 0.5f;
    }

    for (int a = 0; a < 3; a++) {
        if (radius[a] < MAG_CAL_MIN_SPREAD) {
            cal->state = MAG_CAL_DONE_FAIL;
            return MAG_CAL_DONE_FAIL;
        }
    }

    float avg_r = (radius[0] + radius[1] + radius[2]) / 3.0f;

    cal->result.offset_x = offset[0];
    cal->result.offset_y = offset[1];
    cal->result.offset_z = offset[2];
    cal->result.scale_x = avg_r / radius[0];
    cal->result.scale_y = avg_r / radius[1];
    cal->result.scale_z = avg_r / radius[2];

    cal->state = MAG_CAL_DONE_OK;
    return MAG_CAL_DONE_OK;
}

mag_cal_state_t mag_cal_get_state(const mag_cal_t *cal)
{
    if (!cal) return MAG_CAL_IDLE;
    return cal->state;
}

const mag_cal_result_t *mag_cal_get_result(const mag_cal_t *cal)
{
    if (!cal) return NULL;
    return &cal->result;
}

void mag_cal_apply(const mag_cal_result_t *cal,
                   float raw_x, float raw_y, float raw_z,
                   float *out_x, float *out_y, float *out_z)
{
    if (!cal || !out_x || !out_y || !out_z) return;
    *out_x = (raw_x - cal->offset_x) * cal->scale_x;
    *out_y = (raw_y - cal->offset_y) * cal->scale_y;
    *out_z = (raw_z - cal->offset_z) * cal->scale_z;
}

uint16_t mag_cal_sample_count(const mag_cal_t *cal)
{
    if (!cal) return 0;
    return cal->count;
}
