#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "nmea_parser.h"
/* ---------- checksum tests ---------- */

TEST(test_checksum_valid)
{
    ASSERT_TRUE(nmea_verify_checksum("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76"));
    ASSERT_TRUE(nmea_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"));
}

TEST(test_checksum_invalid)
{
    ASSERT_TRUE(!nmea_verify_checksum("$GPGGA,092750.000*FF"));
    ASSERT_TRUE(!nmea_verify_checksum("$GPGGA,no_asterisk"));
    ASSERT_TRUE(!nmea_verify_checksum("GPGGA,no_dollar*00"));
}

TEST(test_checksum_null)
{
    ASSERT_TRUE(!nmea_verify_checksum(NULL));
    ASSERT_TRUE(!nmea_verify_checksum(""));
}

/* ---------- identify tests ---------- */

TEST(test_identify)
{
    ASSERT_EQ(nmea_identify("$GPGGA,stuff"), NMEA_SENTENCE_GGA);
    ASSERT_EQ(nmea_identify("$GNGGA,stuff"), NMEA_SENTENCE_GGA);
    ASSERT_EQ(nmea_identify("$GPRMC,stuff"), NMEA_SENTENCE_RMC);
    ASSERT_EQ(nmea_identify("$GNRMC,stuff"), NMEA_SENTENCE_RMC);
    ASSERT_EQ(nmea_identify("$GPGSA,stuff"), NMEA_SENTENCE_GSA);
    ASSERT_EQ(nmea_identify("$GNGSA,stuff"), NMEA_SENTENCE_GSA);
    ASSERT_EQ(nmea_identify("$GPVTG,stuff"), NMEA_SENTENCE_UNKNOWN);
    ASSERT_EQ(nmea_identify(NULL), NMEA_SENTENCE_UNKNOWN);
    ASSERT_EQ(nmea_identify("short"), NMEA_SENTENCE_UNKNOWN);
}

/* ---------- GGA parse ---------- */

TEST(test_parse_gga)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76",
        &d);
    ASSERT_EQ(e, NMEA_OK);
    ASSERT_EQ(d.fix_quality, 1);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.hdop, 1.03f, 0.01f);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);
}

/* ---------- RMC parse ---------- */

TEST(test_parse_rmc)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
        &d);
    ASSERT_EQ(e, NMEA_OK);
    ASSERT_EQ(d.fix_valid, 1);
    ASSERT_NEAR(d.latitude, 48.1173f, 0.001f);
    ASSERT_NEAR(d.longitude, 11.5167f, 0.001f);
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
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_parse("$GPRMC,123519,V,,,,,,,230394,,,N*7C", &d);
    ASSERT_EQ(d.fix_valid, 0);
}

/* ---------- GSA parse ---------- */

TEST(test_parse_gsa)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39",
        &d);
    ASSERT_EQ(e, NMEA_OK);
    ASSERT_EQ(d.fix_mode, 2);
    ASSERT_NEAR(d.pdop, 2.5f, 0.1f);
    ASSERT_NEAR(d.hdop, 1.3f, 0.1f);
    ASSERT_NEAR(d.vdop, 2.1f, 0.1f);
}

TEST(test_parse_gsa_2d)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_parse("$GPGSA,A,2,04,05,,,,,,,,,,,,3.0,2.0,*34", &d);
    ASSERT_EQ(d.fix_mode, 1);
}

TEST(test_parse_gsa_nofix)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_parse("$GPGSA,A,1,,,,,,,,,,,,,,,,*1E", &d);
    ASSERT_EQ(d.fix_mode, 0);
}

/* ---------- edge cases ---------- */

TEST(test_parse_null_safety)
{
    nmea_data_t d;
    ASSERT_EQ(nmea_parse(NULL, &d), NMEA_ERR_NULL);
    ASSERT_EQ(nmea_parse("$GPGGA,x", NULL), NMEA_ERR_NULL);
}

TEST(test_parse_unknown_sentence)
{
    nmea_data_t d;
    nmea_data_init(&d);
    ASSERT_EQ(nmea_parse("$GPVTG,stuff*00", &d), NMEA_ERR_UNKNOWN);
    ASSERT_EQ(nmea_parse("not_nmea", &d), NMEA_ERR_UNKNOWN);
}

TEST(test_data_init)
{
    nmea_data_t d;
    memset(&d, 0xFF, sizeof(d));
    nmea_data_init(&d);
    ASSERT_EQ(d.fix_valid, 0);
    ASSERT_EQ(d.satellites, 0);
    ASSERT_NEAR(d.latitude, 0.0f, 0.001f);
    ASSERT_NEAR(d.longitude, 0.0f, 0.001f);
    nmea_data_init(NULL);
    ASSERT_TRUE(1);
}

TEST(test_rmc_no_course)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,0.0,,230394,,,A*68", &d);
    ASSERT_EQ(d.course_valid, 0);
}

TEST(test_south_west_coordinates)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,000000,A,3349.500,S,15051.000,W,0.0,0.0,010126,,,A*2A", &d);
    ASSERT_TRUE(d.latitude < 0.0f);
    ASSERT_TRUE(d.longitude < 0.0f);
}

TEST(test_equator_prime_meridian)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010126,,,A*6F", &d);
    ASSERT_NEAR(d.latitude, 0.0f, 0.001f);
    ASSERT_NEAR(d.longitude, 0.0f, 0.001f);
    ASSERT_EQ(d.fix_valid, 1);
}

TEST(test_accumulate_gga_rmc)
{
    nmea_data_t d;
    nmea_data_init(&d);

    nmea_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);

    nmea_parse("$GPRMC,092750,A,5321.6802,N,00630.3372,W,0.0,0.0,230394,,,A*50", &d);
    ASSERT_EQ(d.fix_valid, 1);
    ASSERT_EQ(d.satellites, 8);
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f);
}

TEST(test_reinit_clears)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8);

    nmea_data_init(&d);
    ASSERT_EQ(d.satellites, 0);
    ASSERT_NEAR(d.altitude_m, 0.0f, 0.001f);
}

TEST(test_checksum_lowercase)
{
    ASSERT_TRUE(nmea_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a"));
}

TEST(test_no_comma_in_sentence)
{
    nmea_data_t d;
    nmea_data_init(&d);
    ASSERT_EQ(nmea_parse("$GPGGA*00", &d), NMEA_ERR_UNKNOWN);
}

TEST(test_max_latitude_longitude)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,9000.000,N,18000.000,E,0.0,0.0,010126,,,A*2E", &d);
    ASSERT_NEAR(d.latitude, 90.0f, 0.01f);
    ASSERT_NEAR(d.longitude, 180.0f, 0.01f);
}

TEST(test_year_boundary)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010100,,,A*67", &d);
    ASSERT_EQ(d.year, 2000);

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010179,,,A*6D", &d);
    ASSERT_EQ(d.year, 2079);

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010180,,,A*63", &d);
    ASSERT_EQ(d.year, 1980);

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010199,,,A*63", &d);
    ASSERT_EQ(d.year, 1999);
}

TEST(test_gga_no_fix)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPGGA,,,,,,,0,0,,,,,,,*66", &d);
    ASSERT_EQ(d.fix_quality, 0);
    ASSERT_EQ(d.satellites, 0);
}

TEST(test_rmc_empty_speed_course)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,4807.038,N,01131.000,E,,,010126,,,A*57", &d);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.01f);
    ASSERT_EQ(d.course_valid, 0);
}

TEST(test_alternate_talker_id)
{
    nmea_data_t d;
    nmea_data_init(&d);
    ASSERT_EQ(nmea_identify("$GNGGA,092750,,,,,0,0,,,,,,,*7C"), NMEA_SENTENCE_GGA);
    ASSERT_EQ(nmea_identify("$GLRMC,120000,V,,,,,,,010100,,,N*4F"), NMEA_SENTENCE_RMC);
    ASSERT_EQ(nmea_identify("$BDGSA,A,1,,,,,,,,,,,,,,,*1A"), NMEA_SENTENCE_GSA);
}

TEST(test_truncated_rmc)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_err_t e = nmea_parse("$GPRMC,120000,A,4807.038,N,01131.000,E*64", &d);
    ASSERT_EQ(e, NMEA_OK);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.01f);
}

TEST(test_checksum_truncated_star)
{
    ASSERT_TRUE(!nmea_verify_checksum("$GPGGA*"));
    ASSERT_TRUE(!nmea_verify_checksum("$GPGGA*7"));
}

TEST(test_speed_conversion)
{
    nmea_data_t d;
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,4807.038,N,01131.000,E,100.0,,010126,,,A*2D", &d);
    ASSERT_NEAR(d.speed_kmh, 185.2f, 0.5f);
}

TEST_MAIN("nmea_parser")
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
    RUN(test_max_latitude_longitude);
    RUN(test_year_boundary);
    RUN(test_gga_no_fix);
    RUN(test_rmc_empty_speed_course);
    RUN(test_alternate_talker_id);
    RUN(test_truncated_rmc);
    RUN(test_checksum_truncated_star);
    RUN(test_speed_conversion);
TEST_MAIN_END()

