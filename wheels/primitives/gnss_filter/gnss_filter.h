// SPDX-License-Identifier: MIT
/*
 * gnss_filter — GNSS position EMA filter with quality gate and ZUPT
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef GNSS_FILTER_H
#define GNSS_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    GNSS_FILTER_OK        =  0,
    GNSS_FILTER_ERR_PARAM = -1,
} gnss_filter_err_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t  min_satellites;    /* minimum sats for quality gate */
    float    max_hdop;          /* max HDOP for quality gate */
    float    max_vdop;          /* max VDOP (0 = ignore) */
    uint32_t stable_ms;         /* quality must hold this long before "ready" */
    float    static_alpha;      /* EMA alpha when stationary */
    float    strong_alpha;      /* EMA alpha when ZUPT active (tighter) */
    uint16_t static_speed_cms;  /* below this = stationary */
    uint16_t zupt_motion_mg;    /* IMU motion threshold for ZUPT */
} gnss_filter_cfg_t;

/* ---------- input per update ---------- */

typedef struct {
    float    latitude;
    float    longitude;
    float    altitude;
    uint16_t speed_cms;
    uint8_t  satellites;
    uint8_t  fix_mode;       /* 0=no fix, 2=3D */
    uint8_t  fix_quality;    /* GGA quality indicator */
    float    hdop;
    float    vdop;
    bool     fix_valid;
} gnss_filter_input_t;

/* ---------- instance ---------- */

typedef struct {
    gnss_filter_cfg_t cfg;            /* internal */
    float             filtered_lat;   /* internal */
    float             filtered_lon;   /* internal */
    float             filtered_alt;   /* internal */
    bool              filter_valid;   /* internal */
    uint32_t          quality_ok_ms;  /* internal: accumulated quality time */
    bool              quality_ready;  /* internal */
    bool              last_raw_ok;    /* internal: previous sample was good */
    uint32_t          last_tick_ms;   /* internal */
    bool              started;        /* internal */
} gnss_filter_t;

/* ---------- API ---------- */

/** Returns GNSS_FILTER_ERR_PARAM for a NULL pointer, max_hdop not above 0,
 *  max_vdop below 0, an alpha outside 0..1, or a non-finite float field. A
 *  refused filter is zeroed and never becomes ready.
 *  @thread_safe: no (caller must serialize) @isr_safe: no */
gnss_filter_err_t gnss_filter_init(gnss_filter_t *flt,
                                   const gnss_filter_cfg_t *cfg);

/** A NaN hdop, a NaN vdop while max_vdop is in use, or a non-finite position
 *  counts as a bad sample: the gate closes and stable_ms starts again.
 *  @thread_safe: no @isr_safe: no */
gnss_filter_err_t gnss_filter_update(gnss_filter_t *flt,
                                     const gnss_filter_input_t *input,
                                     uint16_t imu_motion_mg,
                                     uint32_t now_ms);

/** @thread_safe: no @isr_safe: no */
gnss_filter_err_t gnss_filter_reset(gnss_filter_t *flt);

/** Return true when quality gate passed and position valid. @thread_safe: no @isr_safe: no */
bool  gnss_filter_is_ready(const gnss_filter_t *flt);
/** Return filtered latitude (0 if not ready). @thread_safe: no @isr_safe: no */
float gnss_filter_lat(const gnss_filter_t *flt);
/** Return filtered longitude (0 if not ready). @thread_safe: no @isr_safe: no */
float gnss_filter_lon(const gnss_filter_t *flt);
/** Return filtered altitude (0 if not ready). @thread_safe: no @isr_safe: no */
float gnss_filter_alt(const gnss_filter_t *flt);

/* ---------- utility ---------- */

/** Convert a UTC date and time to Unix seconds.
 *  Returns -1 for a month outside 1..12. A parser that checks digits only
 *  passes such a month through, so check for -1 before using the result.
 *  @thread_safe: yes @isr_safe: yes */
int64_t gnss_datetime_to_unix(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t min, uint8_t sec);

#ifdef __cplusplus
}
#endif

#endif /* GNSS_FILTER_H */
