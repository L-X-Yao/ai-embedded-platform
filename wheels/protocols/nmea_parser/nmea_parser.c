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

static int field_int(const char *line, int n)
{
    const char *f = get_field(line, n);
    if (*f == '\0' || *f == ',') return 0;
    return (int)strtol(f, NULL, 10);
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
    d->hour = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    d->min  = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    d->sec  = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
}

static void parse_date(const char *field, nmea_parser_data_t *d)
{
    if (strlen(field) < 6) return;
    if (!is_digit(field[0]) || !is_digit(field[1]) ||
        !is_digit(field[2]) || !is_digit(field[3]) ||
        !is_digit(field[4]) || !is_digit(field[5])) return;
    d->day   = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    d->month = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    uint8_t yy = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
    d->year  = (yy >= 80) ? (uint16_t)(1900 + yy) : (uint16_t)(2000 + yy);
}

/* ---------- sentence parsers ---------- */

static void parse_gga(const char *line, nmea_parser_data_t *d)
{
    d->fix_quality = (uint8_t)field_int(line, 6);
    d->satellites  = (uint8_t)field_int(line, 7);
    d->hdop        = field_float(line, 8);
    d->altitude_m  = field_float(line, 9);
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
    d->speed_kmh = knots * 1.852f;

    const char *course_f = get_field(line, 8);
    if (*course_f && *course_f != ',' && *course_f != '*') {
        d->course_deg = strtof(course_f, NULL);
        d->course_valid = 1;
    } else {
        d->course_valid = 0;
    }

    const char *date_f = get_field(line, 9);
    parse_date(date_f, d);
}

static void parse_gsa(const char *line, nmea_parser_data_t *d)
{
    int mode = field_int(line, 2);
    if (mode == 3)      d->fix_mode = 2;
    else if (mode == 2)  d->fix_mode = 1;
    else                 d->fix_mode = 0;

    d->pdop = field_float(line, 15);
    d->hdop = field_float(line, 16);
    d->vdop = field_float(line, 17);
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
