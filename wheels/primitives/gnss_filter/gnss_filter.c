// SPDX-License-Identifier: MIT
/*
 * gnss_filter — GNSS position EMA filter with quality gate and ZUPT
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "gnss_filter.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static bool quality_raw_ok(const gnss_filter_t *flt,
                           const gnss_filter_input_t *in)
{
    if (!in->fix_valid || in->fix_mode != 2 || in->fix_quality == 0) {
        return false;
    }
    if (in->satellites < flt->cfg.min_satellites) {
        return false;
    }
    if (in->hdop <= 0.0f || in->hdop > flt->cfg.max_hdop) {
        return false;
    }
    if (flt->cfg.max_vdop > 0.0f && in->vdop > 0.0f &&
        in->vdop > flt->cfg.max_vdop) {
        return false;
    }
    return true;
}

/* ---------- public API ---------- */

gnss_filter_err_t gnss_filter_init(gnss_filter_t *flt,
                                   const gnss_filter_cfg_t *cfg)
{
    if (flt == NULL || cfg == NULL) {
        return GNSS_FILTER_ERR_PARAM;
    }
    memset(flt, 0, sizeof(*flt));
    flt->cfg = *cfg;
    return GNSS_FILTER_OK;
}

gnss_filter_err_t gnss_filter_update(gnss_filter_t *flt,
                                     const gnss_filter_input_t *input,
                                     uint16_t imu_motion_mg,
                                     uint32_t now_ms)
{
    if (flt == NULL || input == NULL) {
        return GNSS_FILTER_ERR_PARAM;
    }

    uint32_t elapsed_ms = 0;
    if (flt->started) {
        elapsed_ms = now_ms - flt->last_tick_ms;
    }
    flt->last_tick_ms = now_ms;
    flt->started = true;

    /* Quality gate: accumulate time where raw quality is OK */
    bool raw_ok = quality_raw_ok(flt, input);
    if (!raw_ok) {
        flt->quality_ok_ms = 0;
        flt->quality_ready = false;
    } else {
        flt->quality_ok_ms += elapsed_ms;
        if (flt->quality_ok_ms >= flt->cfg.stable_ms) {
            flt->quality_ready = true;
        }
    }

    /* Position filter: only update when quality gate passed */
    if (!flt->quality_ready) {
        flt->filter_valid = false;
        return GNSS_FILTER_OK;
    }

    bool imu_stationary = (imu_motion_mg < flt->cfg.zupt_motion_mg);

    /* If not yet initialized, or moving (speed + IMU confirm), seed directly */
    if (!flt->filter_valid ||
        (input->speed_cms >= flt->cfg.static_speed_cms && !imu_stationary)) {
        flt->filtered_lat = input->latitude;
        flt->filtered_lon = input->longitude;
        flt->filtered_alt = input->altitude;
        flt->filter_valid = true;
        return GNSS_FILTER_OK;
    }

    /* EMA filter: tighter alpha when ZUPT indicates stationary */
    float alpha = imu_stationary ? flt->cfg.strong_alpha
                                 : flt->cfg.static_alpha;

    flt->filtered_lat += (input->latitude  - flt->filtered_lat) * alpha;
    flt->filtered_lon += (input->longitude - flt->filtered_lon) * alpha;
    flt->filtered_alt += (input->altitude  - flt->filtered_alt) * alpha;

    return GNSS_FILTER_OK;
}

gnss_filter_err_t gnss_filter_reset(gnss_filter_t *flt)
{
    if (flt == NULL) {
        return GNSS_FILTER_ERR_PARAM;
    }
    gnss_filter_cfg_t cfg = flt->cfg;
    memset(flt, 0, sizeof(*flt));
    flt->cfg = cfg;
    return GNSS_FILTER_OK;
}

bool gnss_filter_is_ready(const gnss_filter_t *flt)
{
    if (flt == NULL) return false;
    return flt->quality_ready && flt->filter_valid;
}

float gnss_filter_lat(const gnss_filter_t *flt)
{
    if (flt == NULL || !flt->filter_valid) return 0.0f;
    return flt->filtered_lat;
}

float gnss_filter_lon(const gnss_filter_t *flt)
{
    if (flt == NULL || !flt->filter_valid) return 0.0f;
    return flt->filtered_lon;
}

float gnss_filter_alt(const gnss_filter_t *flt)
{
    if (flt == NULL || !flt->filter_valid) return 0.0f;
    return flt->filtered_alt;
}

/* ---------- utility ---------- */

int64_t gnss_datetime_to_unix(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t min, uint8_t sec)
{
    static const uint16_t days_in_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int32_t days = 0;
    for (uint16_t yr = 1970; yr < year; yr++) {
        bool leap = ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0);
        days += leap ? 366 : 365;
    }

    bool leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
    for (uint8_t mo = 1; mo < month; mo++) {
        days += days_in_month[mo - 1];
        if (mo == 2 && leap) {
            days++;
        }
    }
    days += day - 1;

    return (int64_t)days * 86400
         + (int64_t)hour * 3600
         + (int64_t)min * 60
         + (int64_t)sec;
}
