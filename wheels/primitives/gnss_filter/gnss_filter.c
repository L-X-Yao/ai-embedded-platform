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
#include "contract.h"
#include <float.h>
#include <string.h>

/* ---------- internal helpers ---------- */

/* NaN fails both comparisons and an infinity one of them, so no libm. */
static bool is_finite(float v)
{
    return v >= -FLT_MAX && v <= FLT_MAX;
}

static bool cfg_ok(const gnss_filter_cfg_t *cfg)
{
    return is_finite(cfg->max_hdop) && cfg->max_hdop > 0.0f &&
           is_finite(cfg->max_vdop) && cfg->max_vdop >= 0.0f &&
           cfg->static_alpha >= 0.0f && cfg->static_alpha <= 1.0f &&
           cfg->strong_alpha >= 0.0f && cfg->strong_alpha <= 1.0f;
}

static bool quality_raw_ok(const gnss_filter_t *flt,
                           const gnss_filter_input_t *in)
{
    if (!in->fix_valid || in->fix_mode != 2 || in->fix_quality == 0) {
        return false;
    }
    if (in->satellites < flt->cfg.min_satellites) {
        return false;
    }
    /* Negated so that NaN fails it: api-conventions 2.5 */
    if (!(in->hdop > 0.0f && in->hdop <= flt->cfg.max_hdop)) {
        return false;
    }
    /* vdop 0 means the receiver gave none */
    if (flt->cfg.max_vdop > 0.0f &&
        (in->vdop != in->vdop || in->vdop > flt->cfg.max_vdop)) {
        return false;
    }
    /* One NaN in the EMA would stay there for good */
    if (!is_finite(in->latitude) || !is_finite(in->longitude) ||
        !is_finite(in->altitude)) {
        return false;
    }
    return true;
}

/* Longitude is circular: bring a value back into -180..180. Both inputs to a
 * step are already in range, so one turn is enough. */
static float lon_wrap(float deg)
{
    if (deg > 180.0f) {
        return deg - 360.0f;
    }
    if (deg < -180.0f) {
        return deg + 360.0f;
    }
    return deg;
}

/* ---------- public API ---------- */

gnss_filter_err_t gnss_filter_init(gnss_filter_t *flt,
                                   const gnss_filter_cfg_t *cfg)
{
    REQUIRE(flt != NULL);
    REQUIRE(cfg != NULL);
    if (flt == NULL || cfg == NULL) {
        return GNSS_FILTER_ERR_PARAM;
    }
    memset(flt, 0, sizeof(*flt));
    /* Refused after the memset: a zeroed filter can never pass the gate */
    if (!cfg_ok(cfg)) {
        return GNSS_FILTER_ERR_PARAM;
    }
    flt->cfg = *cfg;
    return GNSS_FILTER_OK;
}

gnss_filter_err_t gnss_filter_update(gnss_filter_t *flt,
                                     const gnss_filter_input_t *input,
                                     uint16_t imu_motion_mg,
                                     uint32_t now_ms)
{
    REQUIRE(flt != NULL);
    REQUIRE(input != NULL);
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
    flt->filtered_lon = lon_wrap(flt->filtered_lon +
        lon_wrap(input->longitude - flt->filtered_lon) * alpha);
    flt->filtered_alt += (input->altitude  - flt->filtered_alt) * alpha;

    return GNSS_FILTER_OK;
}

gnss_filter_err_t gnss_filter_reset(gnss_filter_t *flt)
{
    REQUIRE(flt != NULL);
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
    REQUIRE(flt != NULL);
    if (flt == NULL) return false;
    return flt->quality_ready && flt->filter_valid;
}

float gnss_filter_lat(const gnss_filter_t *flt)
{
    REQUIRE(flt != NULL);
    if (flt == NULL || !flt->filter_valid) return 0.0f;
    return flt->filtered_lat;
}

float gnss_filter_lon(const gnss_filter_t *flt)
{
    REQUIRE(flt != NULL);
    if (flt == NULL || !flt->filter_valid) return 0.0f;
    return flt->filtered_lon;
}

float gnss_filter_alt(const gnss_filter_t *flt)
{
    REQUIRE(flt != NULL);
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

    if (month < 1 || month > 12) {
        return -1;
    }

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
