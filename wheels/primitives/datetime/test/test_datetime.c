// SPDX-License-Identifier: MIT
#include "test_harness.h"
/**
 * @file    test_datetime.c
 * @brief   C unit tests for datetime wheel.
 *
 * Returns 0 on all-pass, non-zero on first failure (pytest checks exit code).
 * Each TEST() prints PASS/FAIL line for human + machine readability.
 */

#include "datetime.h"
#include <string.h>
#include <limits.h>

/* ── Epoch round-trip ───────────────────────────────────────────── */

TEST(test_epoch_zero)
{
    /* Epoch 0 = 2000-01-01 00:00:00 Saturday */
    datetime_t dt;
    ASSERT_EQ(datetime_from_epoch(0, &dt), DATETIME_OK);
    ASSERT_EQ(dt.year, 2000);
    ASSERT_EQ(dt.month, 1);
    ASSERT_EQ(dt.day, 1);
    ASSERT_EQ(dt.hour, 0);
    ASSERT_EQ(dt.minute, 0);
    ASSERT_EQ(dt.second, 0);
    ASSERT_EQ(dt.weekday, 6); /* Saturday */
}

TEST(test_round_trip_epoch_zero)
{
    datetime_t dt;
    ASSERT_EQ(datetime_from_epoch(0, &dt), DATETIME_OK);
    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);
    ASSERT_EQ(epoch, 0);
}

TEST(test_round_trip_known_date)
{
    /* 2026-05-12 14:30:45 */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026;
    dt.month = 5;
    dt.day = 12;
    dt.hour = 14;
    dt.minute = 30;
    dt.second = 45;

    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);

    datetime_t dt2;
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.year, 2026);
    ASSERT_EQ(dt2.month, 5);
    ASSERT_EQ(dt2.day, 12);
    ASSERT_EQ(dt2.hour, 14);
    ASSERT_EQ(dt2.minute, 30);
    ASSERT_EQ(dt2.second, 45);
}

/* ── Leap year boundaries ───────────────────────────────────────── */

TEST(test_leap_year_2000)
{
    /* 2000 is a leap year (divisible by 400) */
    ASSERT_TRUE(datetime_is_leap_year(2000));
    ASSERT_EQ(datetime_days_in_month(2000, 2), 29);
}

TEST(test_not_leap_year_2100)
{
    /* 2100 is NOT a leap year (divisible by 100 but not 400) */
    ASSERT_FALSE(datetime_is_leap_year(2100));
    ASSERT_EQ(datetime_days_in_month(2100, 2), 28);
}

TEST(test_leap_year_2024)
{
    ASSERT_TRUE(datetime_is_leap_year(2024));
    ASSERT_EQ(datetime_days_in_month(2024, 2), 29);
}

TEST(test_not_leap_year_2023)
{
    ASSERT_FALSE(datetime_is_leap_year(2023));
    ASSERT_EQ(datetime_days_in_month(2023, 2), 28);
}

/* ── Month boundaries ───────────────────────────────────────────── */

TEST(test_month_boundaries)
{
    /* Feb 28 -> Mar 1 in non-leap year */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2023; dt.month = 2; dt.day = 28;
    dt.hour = 23; dt.minute = 59; dt.second = 59;

    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);
    epoch += 1; /* +1 second */

    datetime_t dt2;
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.year, 2023);
    ASSERT_EQ(dt2.month, 3);
    ASSERT_EQ(dt2.day, 1);
    ASSERT_EQ(dt2.hour, 0);
    ASSERT_EQ(dt2.minute, 0);
    ASSERT_EQ(dt2.second, 0);
}

TEST(test_feb29_leap_year)
{
    /* Feb 29, 2000 is valid */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2000; dt.month = 2; dt.day = 29;

    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);

    datetime_t dt2;
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.month, 2);
    ASSERT_EQ(dt2.day, 29);
}

/* ── Weekday calculation ────────────────────────────────────────── */

TEST(test_weekday_known_dates)
{
    /* 2000-01-01 = Saturday (6) */
    datetime_t dt;
    ASSERT_EQ(datetime_from_epoch(0, &dt), DATETIME_OK);
    ASSERT_EQ(dt.weekday, 6);

    /* Compute weekday for 2026-05-12 = Tuesday (2) */
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026; dt.month = 5; dt.day = 12;
    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);
    datetime_t dt2;
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.weekday, 2); /* Tuesday */

    /* 2024-01-01 = Monday (1) */
    memset(&dt, 0, sizeof(dt));
    dt.year = 2024; dt.month = 1; dt.day = 1;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.weekday, 1); /* Monday */
}

/* ── add_seconds across midnight/month/year ─────────────────────── */

TEST(test_add_seconds_across_midnight)
{
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026; dt.month = 5; dt.day = 12;
    dt.hour = 23; dt.minute = 59; dt.second = 50;

    ASSERT_EQ(datetime_add_seconds(&dt, 15), DATETIME_OK);
    ASSERT_EQ(dt.year, 2026);
    ASSERT_EQ(dt.month, 5);
    ASSERT_EQ(dt.day, 13);
    ASSERT_EQ(dt.hour, 0);
    ASSERT_EQ(dt.minute, 0);
    ASSERT_EQ(dt.second, 5);
}

TEST(test_add_seconds_across_year)
{
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2025; dt.month = 12; dt.day = 31;
    dt.hour = 23; dt.minute = 59; dt.second = 59;

    ASSERT_EQ(datetime_add_seconds(&dt, 1), DATETIME_OK);
    ASSERT_EQ(dt.year, 2026);
    ASSERT_EQ(dt.month, 1);
    ASSERT_EQ(dt.day, 1);
    ASSERT_EQ(dt.hour, 0);
    ASSERT_EQ(dt.minute, 0);
    ASSERT_EQ(dt.second, 0);
}

TEST(test_subtract_seconds)
{
    /* Subtract across day boundary */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026; dt.month = 3; dt.day = 1;
    dt.hour = 0; dt.minute = 0; dt.second = 0;

    ASSERT_EQ(datetime_add_seconds(&dt, -1), DATETIME_OK);
    ASSERT_EQ(dt.year, 2026);
    ASSERT_EQ(dt.month, 2);
    ASSERT_EQ(dt.day, 28);
    ASSERT_EQ(dt.hour, 23);
    ASSERT_EQ(dt.minute, 59);
    ASSERT_EQ(dt.second, 59);
}

/* ── Invalid date rejection ─────────────────────────────────────── */

TEST(test_invalid_date_rejection)
{
    uint32_t epoch;
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));

    /* Month 0 */
    dt.year = 2026; dt.month = 0; dt.day = 1;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Month 13 */
    dt.month = 13; dt.day = 1;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Feb 29 in non-leap year */
    dt.year = 2023; dt.month = 2; dt.day = 29;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Day 0 */
    dt.year = 2026; dt.month = 1; dt.day = 0;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Day 32 */
    dt.month = 1; dt.day = 32;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Hour 24 */
    dt.day = 1; dt.hour = 24;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Minute 60 */
    dt.hour = 0; dt.minute = 60;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Second 60 */
    dt.minute = 0; dt.second = 60;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);

    /* Year < 2000 */
    dt.year = 1999; dt.month = 1; dt.day = 1; dt.second = 0;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_ERR_PARAM);
}

/* ── NULL pointer rejection ─────────────────────────────────────── */

TEST(test_null_params)
{
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2000; dt.month = 1; dt.day = 1;
    uint32_t epoch;

    ASSERT_EQ(datetime_from_epoch(0, NULL), DATETIME_ERR_PARAM);
    ASSERT_EQ(datetime_to_epoch(NULL, &epoch), DATETIME_ERR_PARAM);
    ASSERT_EQ(datetime_to_epoch(&dt, NULL), DATETIME_ERR_PARAM);
    ASSERT_EQ(datetime_add_seconds(NULL, 0), DATETIME_ERR_PARAM);
}

/* ── Invalid month for days_in_month ────────────────────────────── */

TEST(test_days_in_month_invalid)
{
    ASSERT_EQ(datetime_days_in_month(2026, 0), 0);
    ASSERT_EQ(datetime_days_in_month(2026, 13), 0);
}

/* ── Large epoch (Y2K38 equivalent) ─────────────────────────────── */

TEST(test_large_epoch)
{
    /* Max uint32_t = 4294967295 seconds from 2000-01-01
     * That's about 136 years -> ~2136. Verify it doesn't crash. */
    datetime_t dt;
    ASSERT_EQ(datetime_from_epoch(UINT32_MAX, &dt), DATETIME_OK);
    ASSERT_TRUE(dt.year > 2100);

    /* Round-trip a large but representable epoch: 2099-12-31 23:59:59 */
    memset(&dt, 0, sizeof(dt));
    dt.year = 2099; dt.month = 12; dt.day = 31;
    dt.hour = 23; dt.minute = 59; dt.second = 59;

    uint32_t epoch;
    ASSERT_EQ(datetime_to_epoch(&dt, &epoch), DATETIME_OK);

    datetime_t dt2;
    ASSERT_EQ(datetime_from_epoch(epoch, &dt2), DATETIME_OK);
    ASSERT_EQ(dt2.year, 2099);
    ASSERT_EQ(dt2.month, 12);
    ASSERT_EQ(dt2.day, 31);
    ASSERT_EQ(dt2.hour, 23);
    ASSERT_EQ(dt2.minute, 59);
    ASSERT_EQ(dt2.second, 59);
}

/* ── Underflow on subtract ──────────────────────────────────────── */

TEST(test_subtract_underflow)
{
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2000; dt.month = 1; dt.day = 1;
    /* Subtracting 1 second from epoch 0 should fail */
    ASSERT_EQ(datetime_add_seconds(&dt, -1), DATETIME_ERR_PARAM);
}

/* ── INT32_MIN subtract (signed negation UB guard) ──────────────── */

TEST(test_subtract_int32_min)
{
    /* INT32_MIN = -2147483648. Its magnitude (2147483648) exceeds epoch for
     * dates before ~2068, so the underflow guard must fire. */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2026; dt.month = 1; dt.day = 1;
    /* epoch ~= 820454400, much less than 2147483648 -> must return ERR */
    ASSERT_EQ(datetime_add_seconds(&dt, INT32_MIN), DATETIME_ERR_PARAM);
}

/* ── Positive overflow guard ────────────────────────────────────── */

TEST(test_add_seconds_positive_overflow)
{
    /* A date in 2069 + INT32_MAX seconds would overflow uint32_t.
     * Epoch of 2069-01-01 ~= 2177539200. 2177539200 + 2147483647 > UINT32_MAX. */
    datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    dt.year = 2069; dt.month = 1; dt.day = 1;
    ASSERT_EQ(datetime_add_seconds(&dt, INT32_MAX), DATETIME_ERR_PARAM);
}

TEST_MAIN("datetime")
    RUN(test_epoch_zero);
    RUN(test_round_trip_epoch_zero);
    RUN(test_round_trip_known_date);
    RUN(test_leap_year_2000);
    RUN(test_not_leap_year_2100);
    RUN(test_leap_year_2024);
    RUN(test_not_leap_year_2023);
    RUN(test_month_boundaries);
    RUN(test_feb29_leap_year);
    RUN(test_weekday_known_dates);
    RUN(test_add_seconds_across_midnight);
    RUN(test_add_seconds_across_year);
    RUN(test_subtract_seconds);
    RUN(test_invalid_date_rejection);
    RUN(test_null_params);
    RUN(test_days_in_month_invalid);
    RUN(test_large_epoch);
    RUN(test_subtract_underflow);
    RUN(test_subtract_int32_min);
    RUN(test_add_seconds_positive_overflow);
TEST_MAIN_END()
