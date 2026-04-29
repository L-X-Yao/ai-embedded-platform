#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../nmea_parser.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a)-(b)) < (tol), msg)

/* ---------- checksum tests ---------- */

static void test_checksum_valid(void)
{
    printf("test_checksum_valid\n");
    ASSERT(nmea_verify_checksum("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76"),
           "valid GGA checksum");
    ASSERT(nmea_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"),
           "valid RMC checksum");
}

static void test_checksum_invalid(void)
{
    printf("test_checksum_invalid\n");
    ASSERT(!nmea_verify_checksum("$GPGGA,092750.000*FF"), "bad checksum");
    ASSERT(!nmea_verify_checksum("$GPGGA,no_asterisk"), "missing asterisk");
    ASSERT(!nmea_verify_checksum("GPGGA,no_dollar*00"), "missing dollar");
}

static void test_checksum_null(void)
{
    printf("test_checksum_null\n");
    ASSERT(!nmea_verify_checksum(NULL), "null sentence");
    ASSERT(!nmea_verify_checksum(""), "empty sentence");
}

/* ---------- identify tests ---------- */

static void test_identify(void)
{
    printf("test_identify\n");
    ASSERT_EQ(nmea_identify("$GPGGA,stuff"), NMEA_SENTENCE_GGA, "GP GGA");
    ASSERT_EQ(nmea_identify("$GNGGA,stuff"), NMEA_SENTENCE_GGA, "GN GGA");
    ASSERT_EQ(nmea_identify("$GPRMC,stuff"), NMEA_SENTENCE_RMC, "GP RMC");
    ASSERT_EQ(nmea_identify("$GNRMC,stuff"), NMEA_SENTENCE_RMC, "GN RMC");
    ASSERT_EQ(nmea_identify("$GPGSA,stuff"), NMEA_SENTENCE_GSA, "GP GSA");
    ASSERT_EQ(nmea_identify("$GNGSA,stuff"), NMEA_SENTENCE_GSA, "GN GSA");
    ASSERT_EQ(nmea_identify("$GPVTG,stuff"), NMEA_SENTENCE_UNKNOWN, "VTG unknown");
    ASSERT_EQ(nmea_identify(NULL), NMEA_SENTENCE_UNKNOWN, "null unknown");
    ASSERT_EQ(nmea_identify("short"), NMEA_SENTENCE_UNKNOWN, "short unknown");
}

/* ---------- GGA parse ---------- */

static void test_parse_gga(void)
{
    nmea_data_t d;

    printf("test_parse_gga\n");
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76",
        &d);
    ASSERT_EQ(e, NMEA_OK, "parse ok");
    ASSERT_EQ(d.fix_quality, 1, "quality=1");
    ASSERT_EQ(d.satellites, 8, "sats=8");
    ASSERT_NEAR(d.hdop, 1.03f, 0.01f, "hdop=1.03");
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f, "alt=61.7");
}

/* ---------- RMC parse ---------- */

static void test_parse_rmc(void)
{
    nmea_data_t d;

    printf("test_parse_rmc\n");
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
        &d);
    ASSERT_EQ(e, NMEA_OK, "parse ok");
    ASSERT_EQ(d.fix_valid, 1, "fix valid");
    ASSERT_NEAR(d.latitude, 48.1173f, 0.001f, "lat ~48.117");
    ASSERT_NEAR(d.longitude, 11.5167f, 0.001f, "lon ~11.517");
    ASSERT_NEAR(d.speed_kmh, 22.4f * 1.852f, 0.1f, "speed in kmh");
    ASSERT_NEAR(d.course_deg, 84.4f, 0.1f, "course=84.4");
    ASSERT_EQ(d.course_valid, 1, "course valid");
    ASSERT_EQ(d.hour, 12, "hour=12");
    ASSERT_EQ(d.min, 35, "min=35");
    ASSERT_EQ(d.sec, 19, "sec=19");
    ASSERT_EQ(d.day, 23, "day=23");
    ASSERT_EQ(d.month, 3, "month=3");
    ASSERT_EQ(d.year, 1994, "year=1994");
}

static void test_parse_rmc_void_fix(void)
{
    nmea_data_t d;

    printf("test_parse_rmc_void_fix\n");
    nmea_data_init(&d);

    nmea_parse("$GPRMC,123519,V,,,,,,,230394,,,N*7C", &d);
    ASSERT_EQ(d.fix_valid, 0, "fix invalid");
}

/* ---------- GSA parse ---------- */

static void test_parse_gsa(void)
{
    nmea_data_t d;

    printf("test_parse_gsa\n");
    nmea_data_init(&d);

    nmea_err_t e = nmea_parse(
        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39",
        &d);
    ASSERT_EQ(e, NMEA_OK, "parse ok");
    ASSERT_EQ(d.fix_mode, 2, "3D fix");
    ASSERT_NEAR(d.pdop, 2.5f, 0.1f, "pdop=2.5");
    ASSERT_NEAR(d.hdop, 1.3f, 0.1f, "hdop=1.3");
    ASSERT_NEAR(d.vdop, 2.1f, 0.1f, "vdop=2.1");
}

static void test_parse_gsa_2d(void)
{
    nmea_data_t d;

    printf("test_parse_gsa_2d\n");
    nmea_data_init(&d);

    nmea_parse("$GPGSA,A,2,04,05,,,,,,,,,,,,3.0,2.0,*34", &d);
    ASSERT_EQ(d.fix_mode, 1, "2D fix");
}

static void test_parse_gsa_nofix(void)
{
    nmea_data_t d;

    printf("test_parse_gsa_nofix\n");
    nmea_data_init(&d);

    nmea_parse("$GPGSA,A,1,,,,,,,,,,,,,,,,*1E", &d);
    ASSERT_EQ(d.fix_mode, 0, "no fix");
}

/* ---------- edge cases ---------- */

static void test_parse_null_safety(void)
{
    nmea_data_t d;

    printf("test_parse_null_safety\n");
    ASSERT_EQ(nmea_parse(NULL, &d), NMEA_ERR_NULL, "null sentence");
    ASSERT_EQ(nmea_parse("$GPGGA,x", NULL), NMEA_ERR_NULL, "null data");
}

static void test_parse_unknown_sentence(void)
{
    nmea_data_t d;

    printf("test_parse_unknown_sentence\n");
    nmea_data_init(&d);
    ASSERT_EQ(nmea_parse("$GPVTG,stuff*00", &d), NMEA_ERR_UNKNOWN, "unknown type");
    ASSERT_EQ(nmea_parse("not_nmea", &d), NMEA_ERR_UNKNOWN, "not nmea");
}

static void test_data_init(void)
{
    nmea_data_t d;

    printf("test_data_init\n");
    memset(&d, 0xFF, sizeof(d));
    nmea_data_init(&d);
    ASSERT_EQ(d.fix_valid, 0, "fix_valid=0");
    ASSERT_EQ(d.satellites, 0, "sats=0");
    ASSERT_NEAR(d.latitude, 0.0f, 0.001f, "lat=0");
    ASSERT_NEAR(d.longitude, 0.0f, 0.001f, "lon=0");
    nmea_data_init(NULL);
    ASSERT(1, "null init safe");
}

static void test_rmc_no_course(void)
{
    nmea_data_t d;

    printf("test_rmc_no_course\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,0.0,,230394,,,A*68", &d);
    ASSERT_EQ(d.course_valid, 0, "course not valid when empty");
}

static void test_south_west_coordinates(void)
{
    nmea_data_t d;

    printf("test_south_west_coordinates\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,000000,A,3349.500,S,15051.000,W,0.0,0.0,010126,,,A*2A", &d);
    ASSERT(d.latitude < 0.0f, "south is negative");
    ASSERT(d.longitude < 0.0f, "west is negative");
}

static void test_equator_prime_meridian(void)
{
    nmea_data_t d;

    printf("test_equator_prime_meridian\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010126,,,A*6F", &d);
    ASSERT_NEAR(d.latitude, 0.0f, 0.001f, "equator lat=0");
    ASSERT_NEAR(d.longitude, 0.0f, 0.001f, "prime meridian lon=0");
    ASSERT_EQ(d.fix_valid, 1, "fix valid at origin");
}

static void test_accumulate_gga_rmc(void)
{
    nmea_data_t d;

    printf("test_accumulate_gga_rmc\n");
    nmea_data_init(&d);

    nmea_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8, "sats from GGA");
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f, "alt from GGA");

    nmea_parse("$GPRMC,092750,A,5321.6802,N,00630.3372,W,0.0,0.0,230394,,,A*50", &d);
    ASSERT_EQ(d.fix_valid, 1, "fix from RMC");
    ASSERT_EQ(d.satellites, 8, "sats preserved from GGA");
    ASSERT_NEAR(d.altitude_m, 61.7f, 0.1f, "alt preserved from GGA");
}

static void test_reinit_clears(void)
{
    nmea_data_t d;

    printf("test_reinit_clears\n");
    nmea_data_init(&d);
    nmea_parse("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", &d);
    ASSERT_EQ(d.satellites, 8, "sats set");

    nmea_data_init(&d);
    ASSERT_EQ(d.satellites, 0, "sats cleared after reinit");
    ASSERT_NEAR(d.altitude_m, 0.0f, 0.001f, "alt cleared");
}

static void test_checksum_lowercase(void)
{
    printf("test_checksum_lowercase\n");
    ASSERT(nmea_verify_checksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a"),
           "lowercase hex checksum accepted");
}

static void test_no_comma_in_sentence(void)
{
    nmea_data_t d;

    printf("test_no_comma_in_sentence\n");
    nmea_data_init(&d);
    ASSERT_EQ(nmea_parse("$GPGGA*00", &d), NMEA_ERR_UNKNOWN, "no comma rejected");
}

static void test_max_latitude_longitude(void)
{
    nmea_data_t d;

    printf("test_max_latitude_longitude\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,9000.000,N,18000.000,E,0.0,0.0,010126,,,A*2E", &d);
    ASSERT_NEAR(d.latitude, 90.0f, 0.01f, "max latitude 90");
    ASSERT_NEAR(d.longitude, 180.0f, 0.01f, "max longitude 180");
}

static void test_year_boundary(void)
{
    nmea_data_t d;

    printf("test_year_boundary\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010100,,,A*67", &d);
    ASSERT_EQ(d.year, 2000, "year 00 -> 2000");

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010179,,,A*6D", &d);
    ASSERT_EQ(d.year, 2079, "year 79 -> 2079");

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010180,,,A*63", &d);
    ASSERT_EQ(d.year, 1980, "year 80 -> 1980");

    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,0000.000,N,00000.000,E,0.0,0.0,010199,,,A*63", &d);
    ASSERT_EQ(d.year, 1999, "year 99 -> 1999");
}

static void test_gga_no_fix(void)
{
    nmea_data_t d;

    printf("test_gga_no_fix\n");
    nmea_data_init(&d);
    nmea_parse("$GPGGA,,,,,,,0,0,,,,,,,*66", &d);
    ASSERT_EQ(d.fix_quality, 0, "no fix quality=0");
    ASSERT_EQ(d.satellites, 0, "no fix sats=0");
}

static void test_rmc_empty_speed_course(void)
{
    nmea_data_t d;

    printf("test_rmc_empty_speed_course\n");
    nmea_data_init(&d);
    nmea_parse("$GPRMC,120000,A,4807.038,N,01131.000,E,,,010126,,,A*57", &d);
    ASSERT_NEAR(d.speed_kmh, 0.0f, 0.01f, "empty speed=0");
    ASSERT_EQ(d.course_valid, 0, "empty course invalid");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_checksum_valid();
    test_checksum_invalid();
    test_checksum_null();
    test_identify();
    test_parse_gga();
    test_parse_rmc();
    test_parse_rmc_void_fix();
    test_parse_gsa();
    test_parse_gsa_2d();
    test_parse_gsa_nofix();
    test_parse_null_safety();
    test_parse_unknown_sentence();
    test_data_init();
    test_rmc_no_course();
    test_south_west_coordinates();
    test_accumulate_gga_rmc();
    test_reinit_clears();
    test_equator_prime_meridian();
    test_checksum_lowercase();
    test_no_comma_in_sentence();
    test_max_latitude_longitude();
    test_year_boundary();
    test_gga_no_fix();
    test_rmc_empty_speed_course();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
