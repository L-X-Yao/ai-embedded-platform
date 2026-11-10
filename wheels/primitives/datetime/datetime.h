// SPDX-License-Identifier: MIT
/*
 * datetime -- epoch <-> calendar struct conversion
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Epoch base: 2000-01-01 00:00:00 UTC (not 1970).
 * uint32_t epoch covers 2000-01-01 through 2136-02-07.
 * All functions are pure / reentrant, no global state.
 */

#ifndef DATETIME_H
#define DATETIME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DATETIME_OK        =  0,
    DATETIME_ERR_PARAM = -1,
} datetime_err_t;

/* ---------- calendar struct ---------- */

/**
 * Calendar date/time structure.
 *
 * year:    2000..2099 (valid range)
 * month:   1..12
 * day:     1..31
 * hour:    0..23
 * minute:  0..59
 * second:  0..59
 * weekday: 0=Sunday .. 6=Saturday (computed, not validated on input)
 */
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  weekday;   /* 0=Sun .. 6=Sat */
} datetime_t;

/* ---------- API ---------- */

/**
 * Convert epoch seconds (since 2000-01-01 00:00:00 UTC) to calendar struct.
 *
 * @param epoch  Seconds since 2000-01-01T00:00:00Z.
 * @param out    Output calendar struct. Must not be NULL.
 * @return       DATETIME_OK on success, DATETIME_ERR_PARAM if out is NULL.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
datetime_err_t datetime_from_epoch(uint32_t epoch, datetime_t *out);

/**
 * Convert calendar struct to epoch seconds (since 2000-01-01 00:00:00 UTC).
 *
 * @param dt   Input calendar struct. Must not be NULL. Fields validated.
 * @param out  Output epoch seconds. Must not be NULL.
 * @return     DATETIME_OK on success, DATETIME_ERR_PARAM on NULL or invalid.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
datetime_err_t datetime_to_epoch(const datetime_t *dt, uint32_t *out);

/**
 * Check if a year is a leap year (Gregorian calendar).
 *
 * @param year  Calendar year (e.g. 2000, 2024, 2100).
 * @return      true if leap year.
 * @thread_safe: yes @isr_safe: yes
 */
bool datetime_is_leap_year(uint16_t year);

/**
 * Return the number of days in a given month.
 *
 * @param year   Calendar year (for leap year determination).
 * @param month  Month 1..12.
 * @return       Days in month (28..31), or 0 if month is invalid.
 * @thread_safe: yes @isr_safe: yes
 */
uint8_t datetime_days_in_month(uint16_t year, uint8_t month);

/**
 * Add (or subtract) seconds to/from a datetime struct, in place.
 *
 * @param dt       Calendar struct to modify. Must not be NULL.
 * @param seconds  Seconds to add (negative = subtract).
 * @return         DATETIME_OK on success, DATETIME_ERR_PARAM on NULL or
 *                 if the result would underflow below epoch 0.
 * @thread_safe: yes (no shared state, the caller owns dt) @isr_safe: yes
 */
datetime_err_t datetime_add_seconds(datetime_t *dt, int32_t seconds);

#ifdef __cplusplus
}
#endif

#endif /* DATETIME_H */
