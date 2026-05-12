// SPDX-License-Identifier: MIT
/*
 * datetime -- epoch <-> calendar struct conversion
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Epoch base: 2000-01-01 00:00:00 UTC (Saturday).
 */

#include "datetime.h"
#include "contract.h"

/* Seconds per time unit */
#define SECS_PER_MIN   60U
#define SECS_PER_HOUR  3600U
#define SECS_PER_DAY   86400U

/* Days in each month (non-leap). Index 0 unused. */
static const uint8_t days_in_month_table[13] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* ── Leap year ──────────────────────────────────────────────────── */

bool datetime_is_leap_year(uint16_t year)
{
    if ((year % 4U) != 0U) return false;
    if ((year % 100U) != 0U) return true;
    if ((year % 400U) != 0U) return false;
    return true;
}

/* ── Days in month ──────────────────────────────────────────────── */

uint8_t datetime_days_in_month(uint16_t year, uint8_t month)
{
    if (month < 1U || month > 12U) return 0U;
    if (month == 2U && datetime_is_leap_year(year)) return 29U;
    return days_in_month_table[month];
}

/* ── Weekday (Tomohiko Sakamoto) ────────────────────────────────── */

static uint8_t compute_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    /* Tomohiko Sakamoto's algorithm: returns 0=Sun..6=Sat */
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int y = (int)year;
    if (month < 3U) y -= 1;
    int dow = (y + y / 4 - y / 100 + y / 400 + t[month - 1U] + (int)day) % 7;
    return (uint8_t)dow;
}

/* ── Validate fields ────────────────────────────────────────────── */

static bool validate_fields(const datetime_t *dt)
{
    if (dt->year < 2000U || dt->year > 2099U) return false;
    if (dt->month < 1U || dt->month > 12U) return false;
    uint8_t dim = datetime_days_in_month(dt->year, dt->month);
    if (dt->day < 1U || dt->day > dim) return false;
    if (dt->hour > 23U) return false;
    if (dt->minute > 59U) return false;
    if (dt->second > 59U) return false;
    return true;
}

/* ── Days in year ───────────────────────────────────────────────── */

static uint16_t days_in_year(uint16_t year)
{
    return datetime_is_leap_year(year) ? 366U : 365U;
}

/* ── Epoch -> Calendar ──────────────────────────────────────────── */

datetime_err_t datetime_from_epoch(uint32_t epoch, datetime_t *out)
{
    REQUIRE(out != NULL);
    if (!out) return DATETIME_ERR_PARAM;

    uint32_t rem = epoch;

    /* Compute year */
    uint16_t year = 2000U;
    for (;;) {
        uint32_t diy = (uint32_t)days_in_year(year) * SECS_PER_DAY;
        if (rem < diy) break;
        rem -= diy;
        year++;
    }

    /* Compute month */
    uint8_t month = 1U;
    for (;;) {
        uint32_t dim = (uint32_t)datetime_days_in_month(year, month) * SECS_PER_DAY;
        if (rem < dim) break;
        rem -= dim;
        month++;
    }

    /* Compute day */
    uint8_t day = (uint8_t)(rem / SECS_PER_DAY) + 1U;
    rem %= SECS_PER_DAY;

    /* Compute hour/minute/second */
    uint8_t hour   = (uint8_t)(rem / SECS_PER_HOUR);
    rem %= SECS_PER_HOUR;
    uint8_t minute = (uint8_t)(rem / SECS_PER_MIN);
    uint8_t second = (uint8_t)(rem % SECS_PER_MIN);

    out->year    = year;
    out->month   = month;
    out->day     = day;
    out->hour    = hour;
    out->minute  = minute;
    out->second  = second;
    out->weekday = compute_weekday(year, month, day);

    return DATETIME_OK;
}

/* ── Calendar -> Epoch ──────────────────────────────────────────── */

datetime_err_t datetime_to_epoch(const datetime_t *dt, uint32_t *out)
{
    REQUIRE(dt != NULL);
    REQUIRE(out != NULL);
    if (!dt || !out) return DATETIME_ERR_PARAM;
    if (!validate_fields(dt)) return DATETIME_ERR_PARAM;

    uint32_t days = 0;

    /* Accumulate full years from 2000 to dt->year - 1 */
    for (uint16_t y = 2000U; y < dt->year; y++) {
        days += (uint32_t)days_in_year(y);
    }

    /* Accumulate full months in current year */
    for (uint8_t m = 1U; m < dt->month; m++) {
        days += (uint32_t)datetime_days_in_month(dt->year, m);
    }

    /* Add days within current month (1-based -> 0-based) */
    days += (uint32_t)(dt->day - 1U);

    *out = days * SECS_PER_DAY
         + (uint32_t)dt->hour * SECS_PER_HOUR
         + (uint32_t)dt->minute * SECS_PER_MIN
         + (uint32_t)dt->second;

    return DATETIME_OK;
}

/* ── Add seconds ────────────────────────────────────────────────── */

datetime_err_t datetime_add_seconds(datetime_t *dt, int32_t seconds)
{
    REQUIRE(dt != NULL);
    if (!dt) return DATETIME_ERR_PARAM;
    if (!validate_fields(dt)) return DATETIME_ERR_PARAM;

    /* Convert to epoch, apply offset, convert back */
    uint32_t epoch;
    datetime_err_t err = datetime_to_epoch(dt, &epoch);
    if (err != DATETIME_OK) return err;

    if (seconds < 0) {
        /* Use unsigned negation to avoid UB when seconds == INT32_MIN.
         * (uint32_t)0 - (uint32_t)seconds is well-defined unsigned arithmetic
         * and equals the correct magnitude for all int32_t negative values. */
        uint32_t mag = (uint32_t)0 - (uint32_t)seconds;
        if (mag > epoch) return DATETIME_ERR_PARAM;   /* underflow */
        epoch -= mag;
    } else {
        /* Check for uint32_t overflow before adding. */
        if ((uint32_t)seconds > UINT32_MAX - epoch) return DATETIME_ERR_PARAM;
        epoch += (uint32_t)seconds;
    }

    return datetime_from_epoch(epoch, dt);
}
