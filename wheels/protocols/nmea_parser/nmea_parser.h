// SPDX-License-Identifier: MIT
/*
 * nmea_parser — NMEA 0183 sentence parser (GGA, RMC, GSA)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    NMEA_PARSER_OK           =  0,
    NMEA_PARSER_ERR_NULL     = -1,
    NMEA_PARSER_ERR_CHECKSUM = -2,
    NMEA_PARSER_ERR_UNKNOWN  = -3,
} nmea_parser_err_t;

/* ---------- sentence types ---------- */

typedef enum {
    NMEA_PARSER_SENTENCE_UNKNOWN = 0,
    NMEA_PARSER_SENTENCE_GGA,
    NMEA_PARSER_SENTENCE_RMC,
    NMEA_PARSER_SENTENCE_GSA,
} nmea_parser_sentence_t;

/* ---------- parsed data ---------- */

typedef struct {
    float    latitude_deg;
    float    longitude_deg;
    float    altitude_m;
    float    speed_kmh;
    float    course_deg;
    float    hdop;
    float    pdop;
    float    vdop;
    uint16_t year;
    uint8_t  month, day;
    uint8_t  hour, min, sec;
    uint8_t  fix_quality;
    uint8_t  fix_mode;
    uint8_t  satellites;
    uint8_t  fix_valid;
    uint8_t  course_valid;
} nmea_parser_data_t;

/* ---------- API ---------- */

/** Reset all fields to zero/invalid. @thread_safe: yes */
void nmea_parser_data_init(nmea_parser_data_t *data);

/** Verify NMEA checksum (*HH suffix). @thread_safe: yes (pure) */
bool nmea_parser_verify_checksum(const char *sentence);

/** Identify sentence type (GGA/RMC/GSA). @thread_safe: yes (pure) */
nmea_parser_sentence_t nmea_parser_identify(const char *sentence);

/** Parse one sentence, updating relevant fields in data. @thread_safe: caller_must_lock */
nmea_parser_err_t nmea_parser_parse(const char *sentence, nmea_parser_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* NMEA_PARSER_H */
