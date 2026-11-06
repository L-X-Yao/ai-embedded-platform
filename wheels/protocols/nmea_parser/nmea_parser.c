// SPDX-License-Identifier: MIT
/*
 * nmea_parser — NMEA 0183 sentence parser (GGA, RMC, GSA)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "nmea_parser.h"
#include "contract.h"
#include <string.h>
#include <stdlib.h>

/*
 * A valid checksum says a sentence arrived as sent, not that its values make
 * sense, and strtof takes nan, inf and 1e30 as numbers. A value outside these
 * limits is ignored and the field keeps what it had. NMEA sets few of them, so
 * they are wide, and only have to keep out what no receiver reports.
 */
#define MAX_SPEED_KNOTS 1000.0f     /* the CoCom limit on civil GPS receivers */
#define MAX_COURSE_DEG  360.0f
#define MAX_ABS_ALT_M   1000000.0f
#define MAX_DOP         1000.0f     /* receivers report 99.99 with no geometry */
#define MAX_FIX_QUALITY 9L
#define MAX_SATELLITES  99L         /* the field is two digits */
#define MAX_HOUR        23
#define MAX_MINUTE      59
#define MAX_SECOND      60          /* UTC can have a leap second */
#define MAX_MONTH       12
#define MAX_DAY         31

/* ---------- helpers ---------- */

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static const char *get_field(const char *line, int n)
{
    for (int i = 0; i < n; i++) {
        const char *p = strchr(line, ',');
        if (!p) return "";
        line = p + 1;
    }
    return line;
}

static float field_float(const char *line, int n)
{
    const char *f = get_field(line, n);
    if (*f == '\0' || *f == ',') return 0.0f;
    return strtof(f, NULL);
}

/* Store a field only if it lies in lo..hi. NaN fails both comparisons. */
static void field_float_in(const char *line, int n, float lo, float hi, float *out)
{
    float v = field_float(line, n);
    if (v >= lo && v <= hi) *out = v;
}

/* long, because a cast to int would turn 4294967304 into 8. */
static long field_long(const char *line, int n)
{
    const char *f = get_field(line, n);
    if (*f == '\0' || *f == ',') return 0;
    return strtol(f, NULL, 10);
}

static void field_u8_in(const char *line, int n, long hi, uint8_t *out)
{
    long v = field_long(line, n);
    if (v >= 0 && v <= hi) *out = (uint8_t)v;
}

static char field_char(const char *line, int n)
{
    const char *f = get_field(line, n);
    if (*f == '\0' || *f == ',') return '\0';
    return *f;
}

static float parse_lat(const char *raw, char ns)
{
    float v = strtof(raw, NULL);
    /* ddmm.mmmm: |v| <= 9000. Reject NaN/inf/garbage before the int cast;
     * casting inf to int is undefined behaviour (found by libFuzzer). */
    if (!(v >= -9000.0f && v <= 9000.0f)) return 0.0f;
    int deg = (int)(v / 100.0f);
    float min = v - (float)deg * 100.0f;
    float result = (float)deg + min / 60.0f;
    if (ns == 'S' || ns == 's') result = -result;
    return result;
}

static float parse_lon(const char *raw, char ew)
{
    float v = strtof(raw, NULL);
    /* dddmm.mmmm: |v| <= 18000. Same guard as parse_lat. */
    if (!(v >= -18000.0f && v <= 18000.0f)) return 0.0f;
    int deg = (int)(v / 100.0f);
    float min = v - (float)deg * 100.0f;
    float result = (float)deg + min / 60.0f;
    if (ew == 'W' || ew == 'w') result = -result;
    return result;
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static void parse_time(const char *field, nmea_parser_data_t *d)
{
    if (strlen(field) < 6) return;
    if (!is_digit(field[0]) || !is_digit(field[1]) ||
        !is_digit(field[2]) || !is_digit(field[3]) ||
        !is_digit(field[4]) || !is_digit(field[5])) return;
    int hour   = (field[0] - '0') * 10 + (field[1] - '0');
    int minute = (field[2] - '0') * 10 + (field[3] - '0');
    int sec    = (field[4] - '0') * 10 + (field[5] - '0');
    if (hour > MAX_HOUR || minute > MAX_MINUTE || sec > MAX_SECOND) return;
    d->hour = (uint8_t)hour;
    d->min  = (uint8_t)minute;
    d->sec  = (uint8_t)sec;
}

static void parse_date(const char *field, nmea_parser_data_t *d)
{
    if (strlen(field) < 6) return;
    if (!is_digit(field[0]) || !is_digit(field[1]) ||
        !is_digit(field[2]) || !is_digit(field[3]) ||
        !is_digit(field[4]) || !is_digit(field[5])) return;
    int day   = (field[0] - '0') * 10 + (field[1] - '0');
    int month = (field[2] - '0') * 10 + (field[3] - '0');
    if (day < 1 || day > MAX_DAY || month < 1 || month > MAX_MONTH) return;
    d->day   = (uint8_t)day;
    d->month = (uint8_t)month;
    uint8_t yy = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
    d->year  = (yy >= 80) ? (uint16_t)(1900 + yy) : (uint16_t)(2000 + yy);
}

/* ---------- sentence parsers ---------- */

static void parse_gga(const char *line, nmea_parser_data_t *d)
{
    field_u8_in(line, 6, MAX_FIX_QUALITY, &d->fix_quality);
    field_u8_in(line, 7, MAX_SATELLITES, &d->satellites);
    field_float_in(line, 8, 0.0f, MAX_DOP, &d->hdop);
    field_float_in(line, 9, -MAX_ABS_ALT_M, MAX_ABS_ALT_M, &d->altitude_m);
}

static void parse_rmc(const char *line, nmea_parser_data_t *d)
{
    const char *time_f = get_field(line, 1);
    parse_time(time_f, d);

    char status = field_char(line, 2);
    d->fix_valid = (status == 'A') ? 1 : 0;

    const char *lat_f = get_field(line, 3);
    char ns = field_char(line, 4);
    if (*lat_f && *lat_f != ',')
        d->latitude_deg = parse_lat(lat_f, ns);

    const char *lon_f = get_field(line, 5);
    char ew = field_char(line, 6);
    if (*lon_f && *lon_f != ',')
        d->longitude_deg = parse_lon(lon_f, ew);

    float knots = field_float(line, 7);
    if (knots >= 0.0f && knots <= MAX_SPEED_KNOTS) d->speed_kmh = knots * 1.852f;

    const char *course_f = get_field(line, 8);
    d->course_valid = 0;
    if (*course_f && *course_f != ',' && *course_f != '*') {
        float course = strtof(course_f, NULL);
        if (course >= 0.0f && course <= MAX_COURSE_DEG) {
            d->course_deg = course;
            d->course_valid = 1;
        }
    }

    const char *date_f = get_field(line, 9);
    parse_date(date_f, d);
}

static void parse_gsa(const char *line, nmea_parser_data_t *d)
{
    long mode = field_long(line, 2);
    if (mode == 3)      d->fix_mode = 2;
    else if (mode == 2)  d->fix_mode = 1;
    else                 d->fix_mode = 0;

    field_float_in(line, 15, 0.0f, MAX_DOP, &d->pdop);
    field_float_in(line, 16, 0.0f, MAX_DOP, &d->hdop);
    field_float_in(line, 17, 0.0f, MAX_DOP, &d->vdop);
}

/* ---------- API ---------- */

void nmea_parser_data_init(nmea_parser_data_t *data)
{
    if (!data) return;
    memset(data, 0, sizeof(*data));
}

bool nmea_parser_verify_checksum(const char *sentence)
{
    if (!sentence || *sentence != '$') return false;

    const char *p = sentence + 1;
    uint8_t cksum = 0;
    while (*p && *p != '*')
        cksum ^= (uint8_t)*p++;

    if (*p != '*') return false;
    p++;

    if (*p == '\0') return false;
    int hi = hex_digit(*p++);
    if (*p == '\0') return false;
    int lo = hex_digit(*p);
    if (hi < 0 || lo < 0) return false;

    return cksum == (uint8_t)((hi << 4) | lo);
}

nmea_parser_sentence_t nmea_parser_identify(const char *sentence)
{
    if (!sentence || *sentence != '$') return NMEA_PARSER_SENTENCE_UNKNOWN;

    const char *p = sentence + 1;
    if (strlen(p) < 5) return NMEA_PARSER_SENTENCE_UNKNOWN;

    const char *tag = p + 2;
    if (strncmp(tag, "GGA", 3) == 0) return NMEA_PARSER_SENTENCE_GGA;
    if (strncmp(tag, "RMC", 3) == 0) return NMEA_PARSER_SENTENCE_RMC;
    if (strncmp(tag, "GSA", 3) == 0) return NMEA_PARSER_SENTENCE_GSA;

    return NMEA_PARSER_SENTENCE_UNKNOWN;
}

nmea_parser_err_t nmea_parser_parse(const char *sentence, nmea_parser_data_t *data)
{
    if (!sentence || !data) return NMEA_PARSER_ERR_NULL;

    nmea_parser_sentence_t type = nmea_parser_identify(sentence);
    if (type == NMEA_PARSER_SENTENCE_UNKNOWN) return NMEA_PARSER_ERR_UNKNOWN;

    if (!nmea_parser_verify_checksum(sentence)) return NMEA_PARSER_ERR_CHECKSUM;

    const char *body = strchr(sentence, ',');
    if (!body) return NMEA_PARSER_ERR_UNKNOWN;

    switch (type) {
    case NMEA_PARSER_SENTENCE_GGA: parse_gga(body, data); break;
    case NMEA_PARSER_SENTENCE_RMC: parse_rmc(body, data); break;
    case NMEA_PARSER_SENTENCE_GSA: parse_gsa(body, data); break;
    case NMEA_PARSER_SENTENCE_UNKNOWN:
    default: return NMEA_PARSER_ERR_UNKNOWN;
    }

    return NMEA_PARSER_OK;
}
