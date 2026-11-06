// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nmea_parser.h"
/* ---------- checksum tests ---------- */

TEST(test_checksum_valid)
{
    ASSERT_TRUE(nmea_parser_verify_checksum("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76"));
    ASSERT_TRUE(nmea_parser_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"));
}

TEST(test_checksum_invalid)
{
    ASSERT_TRUE(!nmea_parser_verify_checksum("$GPGGA,092750.000*FF"));
    ASSERT_TRUE(!nmea_parser_verify_checksum("$GPGGA,no_asterisk"));
    ASSERT_TRUE(!nmea_parser_verify_checksum("GPGGA,no_dollar*00"));
}

TEST(test_checksum_null)
{
    ASSERT_TRUE(!nmea_parser_verify_checksum(NULL));
    ASSERT_TRUE(!nmea_parser_verify_checksum(""));
}

/* ---------- identify tests ---------- */

TEST(test_identify)
{
    ASSERT_EQ(nmea_parser_identify("$GPGGA,stuff"), NMEA_PARSER_SENTENCE_GGA);
    ASSERT_EQ(nmea_parser_identify("$GNGGA,stuff"), NMEA_PARSER_SENTENCE_GGA);
    ASSERT_EQ(nmea_parser_identify("$GPRMC,stuff"), NMEA_PARSER_SENTENCE_RMC);
    ASSERT_EQ(nmea_parser_identify("$GNRMC,stuff"), NMEA_PARSER_SENTENCE_RMC);
    ASSERT_EQ(nmea_parser_identify("$GPGSA,stuff"), NMEA_PARSER_SENTENCE_GSA);
    ASSERT_EQ(nmea_parser_identify("$GNGSA,stuff"), NMEA_PARSER_SENTENCE_GSA);
    ASSERT_EQ(nmea_parser_identify("$GPVTG,stuff"), NMEA_PARSER_SENTENCE_UNKNOWN);
    ASSERT_EQ(nmea_parser_identify(NULL), NMEA_PARSER_SENTENCE_UNKNOWN);
    ASSERT_EQ(nmea_parser_identify("short"), NMEA_PARSER_SENTENCE_UNKNOWN);
}

/* ---------- GGA parse ---------- */

TEST(test_parse_gga)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_err_t e = nmea_parser_parse(
        "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76",
        &d);
    ASSERT_EQ(e, NMEA_PARSER_OK);
    ASSERT_EQ(d.fix_quality, 1);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.hdop, 1.03f, 0.01f);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);
}

/* ---------- RMC parse ---------- */

TEST(test_parse_rmc)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_err_t e = nmea_parser_parse(
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
        &d);
    ASSERT_EQ(e, NMEA_PARSER_OK);
    ASSERT_EQ(d.fix_valid, 1);
    ASSERT_NEAR(d.latitude_deg, 48.1173f, 0.001f);
    ASSERT_NEAR(d.longitude_deg, 11.5167f, 0.001f);
    ASSERT_NEAR(d.speed_kmh, 22.4f * 1.852f, 0.1f);
    ASSERT_NEAR(d.course_deg, 84.4f, 0.1f);
    ASSERT_EQ(d.course_valid, 1);
    ASSERT_EQ(d.hour, 12);
    ASSERT_EQ(d.min, 35);
    ASSERT_EQ(d.sec, 19);
    ASSERT_EQ(d.day, 23);
    ASSERT_EQ(d.month, 3);
    ASSERT_EQ(d.year, 1994);
}

TEST(test_parse_rmc_void_fix)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_parse("$GPRMC,123519,V,,,,,,,230394,,,N*51", &d);
    ASSERT_EQ(d.fix_valid, 0);
}

/* ---------- GSA parse ---------- */

TEST(test_parse_gsa)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_err_t e = nmea_parser_parse(
        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39",
        &d);
    ASSERT_EQ(e, NMEA_PARSER_OK);
    ASSERT_EQ(d.fix_mode, 2);
    ASSERT_NEAR(d.pdop, 2.5f, 0.1f);
    ASSERT_NEAR(d.hdop, 1.3f, 0.1f);
    ASSERT_NEAR(d.vdop, 2.1f, 0.1f);
}

TEST(test_parse_gsa_2d)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_parse("$GPGSA,A,2,04,05,,,,,,,,,,,,3.0,2.0,*31", &d);
    ASSERT_EQ(d.fix_mode, 1);
}

TEST(test_parse_gsa_nofix)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_parse("$GPGSA,A,1,,,,,,,,,,,,,,,,*32", &d);
    ASSERT_EQ(d.fix_mode, 0);
}

/* ---------- edge cases ---------- */

TEST(test_parse_null_safety)
{
    nmea_parser_data_t d;
    ASSERT_EQ(nmea_parser_parse(NULL, &d), NMEA_PARSER_ERR_NULL);
    ASSERT_EQ(nmea_parser_parse("$GPGGA,x", NULL), NMEA_PARSER_ERR_NULL);
}

TEST(test_parse_unknown_sentence)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    ASSERT_EQ(nmea_parser_parse("$GPVTG,stuff*00", &d), NMEA_PARSER_ERR_UNKNOWN);
    ASSERT_EQ(nmea_parser_parse("not_nmea", &d), NMEA_PARSER_ERR_UNKNOWN);
}

TEST(test_data_init)
{
    nmea_parser_data_t d;
    memset(&d, 0xFF, sizeof(d));
    nmea_parser_data_init(&d);
    ASSERT_EQ(d.fix_valid, 0);
    ASSERT_EQ(d.satellites, 0);
    ASSERT_NEAR(d.latitude_deg, 0.0f, 0.001f);
    ASSERT_NEAR(d.longitude_deg, 0.0f, 0.001f);
    nmea_parser_data_init(NULL);
    ASSERT_TRUE(1);
}

TEST(test_rmc_no_course)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,0.0,,230394,,,A*5E", &d);
    ASSERT_EQ(d.course_valid, 0);
}

TEST(test_south_west_coordinates)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,000000,A,3349.500,S,15051.000,W,0.0,0.0,010126,,,A*73", &d);
    ASSERT_TRUE(d.latitude_deg < 0.0f);
    ASSERT_TRUE(d.longitude_deg < 0.0f);
}

TEST(test_equator_prime_meridian)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010126,,,A*77", &d);
    ASSERT_NEAR(d.latitude_deg, 0.0f, 0.001f);
    ASSERT_NEAR(d.longitude_deg, 0.0f, 0.001f);
    ASSERT_EQ(d.fix_valid, 1);
}

TEST(test_accumulate_gga_rmc)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);

    nmea_parser_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);

    nmea_parser_parse("$GPRMC,092750,A,5321.6802,N,00630.3372,W,0.0,0.0,230394,,,A*6D", &d);
    ASSERT_EQ(d.fix_valid, 1);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);
}

TEST(test_reinit_clears)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8);

    nmea_parser_data_init(&d);
    ASSERT_EQ(d.satellites, 0);
    ASSERT_NEAR(d.altitude_m, 0.0f, 0.001f);
}

TEST(test_checksum_lowercase)
{
    ASSERT_TRUE(nmea_parser_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a"));
}

TEST(test_no_comma_in_sentence)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    ASSERT_EQ(nmea_parser_parse("$GPGGA*56", &d), NMEA_PARSER_ERR_UNKNOWN);
}

TEST(test_parse_bad_checksum_rejected)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    ASSERT_EQ(nmea_parser_parse(
        "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*FF",
        &d), NMEA_PARSER_ERR_CHECKSUM);
    ASSERT_EQ(d.fix_quality, 0);
}

TEST(test_max_latitude_longitude)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,9000.000,N,18000.000,E,0.0,0.0,010126,,,A*77", &d);
    ASSERT_NEAR(d.latitude_deg, 90.0f, 0.01f);
    ASSERT_NEAR(d.longitude_deg, 180.0f, 0.01f);
}

TEST(test_year_boundary)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010100,,,A*73", &d);
    ASSERT_EQ(d.year, 2000);

    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010179,,,A*7D", &d);
    ASSERT_EQ(d.year, 2079);

    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010180,,,A*7B", &d);
    ASSERT_EQ(d.year, 1980);

    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010199,,,A*73", &d);
    ASSERT_EQ(d.year, 1999);
}

TEST(test_gga_no_fix)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPGGA,,,,,,,0,0,,,,,,,*7A", &d);
    ASSERT_EQ(d.fix_quality, 0);
    ASSERT_EQ(d.satellites, 0);
}

TEST(test_rmc_non_finite_lat_lon_rejected)
{
    /* libFuzzer 2026-11-05: strtof("9e99") is inf and (int)(inf / 100.0f)
     * is undefined behaviour. Out-of-range coordinates must parse to 0. */
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,9e99,N,01131.000,E,,,010126,,,A*37", &d);
    ASSERT_NEAR(d.latitude_deg, 0.0f, 1e-6f);
    ASSERT_NEAR(d.longitude_deg, 11.5166667f, 1e-4f);

    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,4807.038,N,1e40,E,,,010126,,,A*09", &d);
    ASSERT_NEAR(d.latitude_deg, 48.1173f, 1e-4f);
    ASSERT_NEAR(d.longitude_deg, 0.0f, 1e-6f);

    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,nan,N,inf,E,,,010126,,,A*47", &d);
    ASSERT_NEAR(d.latitude_deg, 0.0f, 1e-6f);
    ASSERT_NEAR(d.longitude_deg, 0.0f, 1e-6f);
}

TEST(test_rmc_empty_speed_course)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,4807.038,N,01131.000,E,,,010126,,,A*75", &d);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.01f);
    ASSERT_EQ(d.course_valid, 0);
}

TEST(test_alternate_talker_id)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    ASSERT_EQ(nmea_parser_identify("$GNGGA,092750,,,,,0,0,,,,,,,*7C"), NMEA_PARSER_SENTENCE_GGA);
    ASSERT_EQ(nmea_parser_identify("$GLRMC,120000,V,,,,,,,010100,,,N*4F"), NMEA_PARSER_SENTENCE_RMC);
    ASSERT_EQ(nmea_parser_identify("$BDGSA,A,1,,,,,,,,,,,,,,,*1A"), NMEA_PARSER_SENTENCE_GSA);
}

TEST(test_truncated_rmc)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_err_t e = nmea_parser_parse("$GPRMC,120000,A,4807.038,N,01131.000,E*30", &d);
    ASSERT_EQ(e, NMEA_PARSER_OK);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.01f);
}

TEST(test_checksum_truncated_star)
{
    ASSERT_TRUE(!nmea_parser_verify_checksum("$GPGGA*"));
    ASSERT_TRUE(!nmea_parser_verify_checksum("$GPGGA*7"));
}

TEST(test_speed_conversion)
{
    nmea_parser_data_t d;
    nmea_parser_data_init(&d);
    nmea_parser_parse("$GPRMC,120000,A,4807.038,N,01131.000,E,100.0,,010126,,,A*5A", &d);
    ASSERT_NEAR(d.speed_kmh, 185.2f, 0.5f);
}

/* ---------- values a valid checksum cannot vouch for ---------- */

/* Build "$GPxxx,<head><value><tail>" with a correct checksum and parse it. */
static nmea_parser_err_t parse_field(const char *head, const char *value,
                                     const char *tail, nmea_parser_data_t *d)
{
    char body[128];
    char line[136];
    uint8_t sum = 0;

    snprintf(body, sizeof(body), "%s%s%s", head, value, tail);
    for (const char *p = body + 1; *p; p++) {
        sum ^= (uint8_t)*p;
    }
    snprintf(line, sizeof(line), "%s*%02X", body, (unsigned)sum);
    return nmea_parser_parse(line, d);
}

#define RMC_SPEED_HEAD  "$GPRMC,123519,A,4807.038,N,01131.000,E,"
#define RMC_COURSE_HEAD "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,"
#define RMC_DATE_HEAD   "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,"
#define RMC_TIME_TAIL   ",A,4807.038,N,01131.000,E,022.4,084.4,230394,,"
#define GGA_QUALITY_HEAD "$GPGGA,123519,4807.038,N,01131.000,E,"
#define GGA_SATS_HEAD   "$GPGGA,123519,4807.038,N,01131.000,E,1,"
#define GGA_HDOP_HEAD   "$GPGGA,123519,4807.038,N,01131.000,E,1,08,"
#define GGA_ALT_HEAD    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,"
#define GSA_HEAD        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,"

static void seed_good_fix(nmea_parser_data_t *d)
{
    nmea_parser_data_init(d);
    (void)parse_field(RMC_SPEED_HEAD, "022.4", ",084.4,230394,,", d);
    (void)parse_field(GGA_ALT_HEAD, "545.4", ",M,46.9,M,,", d);
    (void)parse_field(GSA_HEAD, "2.5", ",1.3,2.1", d);
}

TEST(test_rmc_bad_speed_keeps_the_old_value)
{
    static const char *const bad[] = {
        "nan", "inf", "-inf", "1e30", "-1.0", "1000.5",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(parse_field(RMC_SPEED_HEAD, bad[i], ",084.4,230394,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_NEAR(d.speed_kmh, 22.4f * 1.852f, 0.01f);
    }
}

TEST(test_rmc_speed_limits)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(RMC_SPEED_HEAD, "1000.0", ",084.4,230394,,", &d);
    ASSERT_NEAR(d.speed_kmh, 1852.0f, 0.1f);
    (void)parse_field(RMC_SPEED_HEAD, "0.0", ",084.4,230394,,", &d);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.001f);
}

TEST(test_rmc_bad_course_is_not_valid)
{
    static const char *const bad[] = {
        "nan", "inf", "-inf", "1e30", "-1.0", "360.5",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(d.course_valid, 1);
        ASSERT_EQ(parse_field(RMC_COURSE_HEAD, bad[i], ",230394,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_EQ(d.course_valid, 0);
        ASSERT_NEAR(d.course_deg, 84.4f, 0.01f);
    }
}

TEST(test_rmc_course_limits)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(RMC_COURSE_HEAD, "360.0", ",230394,,", &d);
    ASSERT_EQ(d.course_valid, 1);
    ASSERT_NEAR(d.course_deg, 360.0f, 0.001f);
    (void)parse_field(RMC_COURSE_HEAD, "0.0", ",230394,,", &d);
    ASSERT_EQ(d.course_valid, 1);
    ASSERT_NEAR(d.course_deg, 0.0f, 0.001f);
}

TEST(test_gga_bad_altitude_keeps_the_old_value)
{
    static const char *const bad[] = {
        "nan", "inf", "-inf", "1e30", "-1e30", "1000001.0", "-1000001.0",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(parse_field(GGA_ALT_HEAD, bad[i], ",M,46.9,M,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_NEAR(d.altitude_m, 545.4f, 0.01f);
    }
}

TEST(test_gga_altitude_below_sea_level_is_fine)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(GGA_ALT_HEAD, "-430.5", ",M,46.9,M,,", &d);
    ASSERT_NEAR(d.altitude_m, -430.5f, 0.01f);
}

TEST(test_bad_dop_keeps_the_old_value)
{
    static const char *const bad[] = {
        "nan", "inf", "-inf", "1e30", "-1.0", "1000.5",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(parse_field(GGA_HDOP_HEAD, bad[i], ",545.4,M,46.9,M,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_NEAR(d.hdop, 1.3f, 0.001f);

        ASSERT_EQ(parse_field(GSA_HEAD, bad[i], ",1.3,2.1", &d), NMEA_PARSER_OK);
        ASSERT_NEAR(d.pdop, 2.5f, 0.001f);

        ASSERT_EQ(parse_field(GSA_HEAD "2.5,", bad[i], ",2.1", &d), NMEA_PARSER_OK);
        ASSERT_NEAR(d.hdop, 1.3f, 0.001f);

        ASSERT_EQ(parse_field(GSA_HEAD "2.5,1.3,", bad[i], "", &d), NMEA_PARSER_OK);
        ASSERT_NEAR(d.vdop, 2.1f, 0.001f);
    }
}

TEST(test_dop_of_a_receiver_with_no_geometry_is_fine)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(GSA_HEAD, "99.99", ",99.99,99.99", &d);
    ASSERT_NEAR(d.pdop, 99.99f, 0.001f);
    ASSERT_NEAR(d.hdop, 99.99f, 0.001f);
    ASSERT_NEAR(d.vdop, 99.99f, 0.001f);
}

TEST(test_rmc_bad_time_keeps_the_old_time)
{
    static const char *const bad[] = { "240000", "126000", "123561", "999999" };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(parse_field("$GPRMC,", bad[i], RMC_TIME_TAIL, &d),
                  NMEA_PARSER_OK);
        ASSERT_EQ(d.hour, 12);
        ASSERT_EQ(d.min, 35);
        ASSERT_EQ(d.sec, 19);
    }
}

TEST(test_rmc_time_limits)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field("$GPRMC,", "000000", RMC_TIME_TAIL, &d);
    ASSERT_EQ(d.hour, 0);
    ASSERT_EQ(d.min, 0);
    ASSERT_EQ(d.sec, 0);

    /* UTC can have a 60th second. */
    (void)parse_field("$GPRMC,", "235960", RMC_TIME_TAIL, &d);
    ASSERT_EQ(d.hour, 23);
    ASSERT_EQ(d.min, 59);
    ASSERT_EQ(d.sec, 60);
}

TEST(test_rmc_bad_date_keeps_the_old_date)
{
    static const char *const bad[] = {
        "019926", "011326", "010026", "001026", "321026", "000000",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        ASSERT_EQ(parse_field(RMC_DATE_HEAD, bad[i], ",,", &d), NMEA_PARSER_OK);
        ASSERT_EQ(d.day, 23);
        ASSERT_EQ(d.month, 3);
        ASSERT_EQ(d.year, 1994);
        ASSERT_EQ(d.fix_valid, 1);
    }
}

TEST(test_rmc_date_limits)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(RMC_DATE_HEAD, "311226", ",,", &d);
    ASSERT_EQ(d.day, 31);
    ASSERT_EQ(d.month, 12);
    ASSERT_EQ(d.year, 2026);
    (void)parse_field(RMC_DATE_HEAD, "010126", ",,", &d);
    ASSERT_EQ(d.day, 1);
    ASSERT_EQ(d.month, 1);
}

TEST(test_gga_bad_satellites_keep_the_old_count)
{
    /* 300 used to wrap to 44, and the last one wraps to 8 through an int. */
    static const char *const bad[] = { "300", "256", "100", "-1", "4294967304" };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        (void)parse_field(GGA_SATS_HEAD, "06", ",0.9,545.4,M,46.9,M,,", &d);
        ASSERT_EQ(parse_field(GGA_SATS_HEAD, bad[i], ",0.9,545.4,M,46.9,M,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_EQ(d.satellites, 6);
    }
}

TEST(test_gga_satellite_limit)
{
    nmea_parser_data_t d;

    seed_good_fix(&d);
    (void)parse_field(GGA_SATS_HEAD, "99", ",0.9,545.4,M,46.9,M,,", &d);
    ASSERT_EQ(d.satellites, 99);
}

TEST(test_gga_bad_quality_keeps_the_old_value)
{
    static const char *const bad[] = { "300", "257", "10", "-1", "4294967297" };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_good_fix(&d);
        (void)parse_field(GGA_QUALITY_HEAD, "2", ",08,0.9,545.4,M,46.9,M,,", &d);
        ASSERT_EQ(parse_field(GGA_QUALITY_HEAD, bad[i], ",08,0.9,545.4,M,46.9,M,,", &d),
                  NMEA_PARSER_OK);
        ASSERT_EQ(d.fix_quality, 2);
    }
}

TEST(test_gsa_mode_does_not_wrap)
{
    nmea_parser_data_t d;

    /* 4294967299 is 3 once it goes through an int. */
    seed_good_fix(&d);
    (void)parse_field("$GPGSA,A,", "4294967299", ",04,,,,,,,,,,,,2.5,1.3,2.1", &d);
    ASSERT_EQ(d.fix_mode, 0);
}

TEST(test_bad_field_leaves_the_rest_of_the_sentence)
{
    nmea_parser_data_t d;

    nmea_parser_data_init(&d);
    (void)parse_field(RMC_SPEED_HEAD, "inf", ",084.4,230394,,", &d);
    ASSERT_EQ(d.fix_valid, 1);
    ASSERT_NEAR(d.latitude_deg, 48.1173f, 1e-4f);
    ASSERT_NEAR(d.course_deg, 84.4f, 0.01f);
    ASSERT_EQ(d.day, 23);
}

/* ---------- GGA position ---------- */

#define GGA_AFTER_POSITION ",1,08,0.9,545.4,M,46.9,M,,"

/* An old position far from the one the GGA sentences below carry. */
static void seed_position(nmea_parser_data_t *d)
{
    nmea_parser_data_init(d);
    (void)parse_field("$GPRMC,123519,A,3000.000,N,06000.000,E,", "022.4",
                      ",084.4,230394,,", d);
}

TEST(test_gga_takes_position)
{
    nmea_parser_data_t d;

    nmea_parser_data_init(&d);
    ASSERT_EQ(parse_field("$GPGGA,123519,4807.038,N,01131.000,E", "",
                          GGA_AFTER_POSITION, &d), NMEA_PARSER_OK);
    ASSERT_NEAR(d.latitude_deg, 48.1173f, 1e-4f);
    ASSERT_NEAR(d.longitude_deg, 11.5167f, 1e-4f);
}

TEST(test_gga_south_west_is_negative)
{
    nmea_parser_data_t d;

    nmea_parser_data_init(&d);
    (void)parse_field("$GPGGA,123519,3352.000,S,15112.000,W", "",
                      GGA_AFTER_POSITION, &d);
    ASSERT_NEAR(d.latitude_deg, -33.8667f, 1e-4f);
    ASSERT_NEAR(d.longitude_deg, -151.2f, 1e-4f);
}

TEST(test_gga_position_limits)
{
    nmea_parser_data_t d;

    seed_position(&d);
    (void)parse_field("$GPGGA,123519,9000.000,N,18000.000,E", "",
                      GGA_AFTER_POSITION, &d);
    ASSERT_NEAR(d.latitude_deg, 90.0f, 1e-4f);
    ASSERT_NEAR(d.longitude_deg, 180.0f, 1e-4f);

    (void)parse_field("$GPGGA,123519,0000.000,N,00000.000,E", "",
                      GGA_AFTER_POSITION, &d);
    ASSERT_NEAR(d.latitude_deg, 0.0f, 1e-6f);
    ASSERT_NEAR(d.longitude_deg, 0.0f, 1e-6f);
}

TEST(test_gga_leaves_the_time_alone)
{
    /* Time and date come from RMC only: a consumer pairs them with the
     * moment the RMC arrived. */
    nmea_parser_data_t d;

    seed_position(&d);
    (void)parse_field("$GPGGA,235958,4807.038,N,01131.000,E", "",
                      GGA_AFTER_POSITION, &d);
    ASSERT_EQ(d.hour, 12);
    ASSERT_EQ(d.min, 35);
    ASSERT_EQ(d.sec, 19);
    ASSERT_EQ(d.day, 23);
    ASSERT_EQ(d.month, 3);
    ASSERT_EQ(d.year, 1994);

    /* A GGA-only stream has a position and no time. */
    nmea_parser_data_init(&d);
    (void)parse_field("$GPGGA,235958,4807.038,N,01131.000,E", "",
                      GGA_AFTER_POSITION, &d);
    ASSERT_EQ(d.hour, 0);
    ASSERT_EQ(d.fix_valid, 0);
}

TEST(test_gga_empty_position_keeps_the_old_value)
{
    nmea_parser_data_t d;

    seed_position(&d);
    ASSERT_EQ(parse_field("$GPGGA,123520,,,,,0,00,99.99,,,,,,", "", "", &d),
              NMEA_PARSER_OK);
    ASSERT_NEAR(d.latitude_deg, 30.0f, 1e-4f);
    ASSERT_NEAR(d.longitude_deg, 60.0f, 1e-4f);
}

TEST(test_gga_bad_latitude_keeps_the_old_value)
{
    static const char *const bad[] = {
        "", "nan", "inf", "-inf", "1e30", "9000.5", "9100.000", "-4807.038",
        "abc", "4860.000",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_position(&d);
        ASSERT_EQ(parse_field("$GPGGA,123519,", bad[i],
                              ",N,01131.000,E" GGA_AFTER_POSITION, &d),
                  NMEA_PARSER_OK);
        ASSERT_NEAR(d.latitude_deg, 30.0f, 1e-4f);
        ASSERT_NEAR(d.longitude_deg, 11.5167f, 1e-4f);
    }
}

TEST(test_gga_bad_longitude_keeps_the_old_value)
{
    static const char *const bad[] = {
        "", "nan", "inf", "-inf", "1e30", "18000.5", "18100.000", "-01131.000",
        "abc", "01160.000",
    };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_position(&d);
        ASSERT_EQ(parse_field("$GPGGA,123519,4807.038,N,", bad[i],
                              ",E" GGA_AFTER_POSITION, &d), NMEA_PARSER_OK);
        ASSERT_NEAR(d.latitude_deg, 48.1173f, 1e-4f);
        ASSERT_NEAR(d.longitude_deg, 60.0f, 1e-4f);
    }
}

TEST(test_gga_bad_hemisphere_keeps_the_old_value)
{
    /* Without N, S, E or W the sign of the value is not known. */
    static const char *const bad[] = { "", "X", "E", "1" };
    static const char *const bad_ew[] = { "", "X", "N", "1" };
    nmea_parser_data_t d;

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        seed_position(&d);
        (void)parse_field("$GPGGA,123519,4807.038,", bad[i],
                          ",01131.000,E" GGA_AFTER_POSITION, &d);
        ASSERT_NEAR(d.latitude_deg, 30.0f, 1e-4f);
        ASSERT_NEAR(d.longitude_deg, 11.5167f, 1e-4f);

        seed_position(&d);
        (void)parse_field("$GPGGA,123519,4807.038,N,01131.000,", bad_ew[i],
                          GGA_AFTER_POSITION, &d);
        ASSERT_NEAR(d.latitude_deg, 48.1173f, 1e-4f);
        ASSERT_NEAR(d.longitude_deg, 60.0f, 1e-4f);
    }
}

TEST_MAIN("nmea_parser")
    RUN(test_gga_takes_position);
    RUN(test_gga_south_west_is_negative);
    RUN(test_gga_position_limits);
    RUN(test_gga_leaves_the_time_alone);
    RUN(test_gga_empty_position_keeps_the_old_value);
    RUN(test_gga_bad_latitude_keeps_the_old_value);
    RUN(test_gga_bad_longitude_keeps_the_old_value);
    RUN(test_gga_bad_hemisphere_keeps_the_old_value);
    RUN(test_rmc_bad_speed_keeps_the_old_value);
    RUN(test_rmc_speed_limits);
    RUN(test_rmc_bad_course_is_not_valid);
    RUN(test_rmc_course_limits);
    RUN(test_gga_bad_altitude_keeps_the_old_value);
    RUN(test_gga_altitude_below_sea_level_is_fine);
    RUN(test_bad_dop_keeps_the_old_value);
    RUN(test_dop_of_a_receiver_with_no_geometry_is_fine);
    RUN(test_rmc_bad_time_keeps_the_old_time);
    RUN(test_rmc_time_limits);
    RUN(test_rmc_bad_date_keeps_the_old_date);
    RUN(test_rmc_date_limits);
    RUN(test_gga_bad_satellites_keep_the_old_count);
    RUN(test_gga_satellite_limit);
    RUN(test_gga_bad_quality_keeps_the_old_value);
    RUN(test_gsa_mode_does_not_wrap);
    RUN(test_bad_field_leaves_the_rest_of_the_sentence);
    RUN(test_checksum_valid);
    RUN(test_checksum_invalid);
    RUN(test_checksum_null);
    RUN(test_identify);
    RUN(test_parse_gga);
    RUN(test_parse_rmc);
    RUN(test_parse_rmc_void_fix);
    RUN(test_parse_gsa);
    RUN(test_parse_gsa_2d);
    RUN(test_parse_gsa_nofix);
    RUN(test_parse_null_safety);
    RUN(test_parse_unknown_sentence);
    RUN(test_data_init);
    RUN(test_rmc_no_course);
    RUN(test_south_west_coordinates);
    RUN(test_accumulate_gga_rmc);
    RUN(test_reinit_clears);
    RUN(test_equator_prime_meridian);
    RUN(test_checksum_lowercase);
    RUN(test_no_comma_in_sentence);
    RUN(test_parse_bad_checksum_rejected);
    RUN(test_max_latitude_longitude);
    RUN(test_year_boundary);
    RUN(test_gga_no_fix);
    RUN(test_rmc_non_finite_lat_lon_rejected);
    RUN(test_rmc_empty_speed_course);
    RUN(test_alternate_talker_id);
    RUN(test_truncated_rmc);
    RUN(test_checksum_truncated_star);
    RUN(test_speed_conversion);
TEST_MAIN_END()

