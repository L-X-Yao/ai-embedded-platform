/*
 * nmea_parser — NMEA 0183 sentence parser (GGA, RMC, GSA)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
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
    NMEA_OK = 0,
    NMEA_ERR_NULL,
    NMEA_ERR_CHECKSUM,
    NMEA_ERR_UNKNOWN,
} nmea_err_t;

/* ---------- sentence types ---------- */

typedef enum {
    NMEA_SENTENCE_UNKNOWN = 0,
    NMEA_SENTENCE_GGA,
    NMEA_SENTENCE_RMC,
    NMEA_SENTENCE_GSA,
} nmea_sentence_t;

/* ---------- parsed data ---------- */

typedef struct {
    float    latitude;
    float    longitude;
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
} nmea_data_t;

/* ---------- API ---------- */

/** Reset all fields to zero/invalid. @thread_safe: yes */
void nmea_data_init(nmea_data_t *data);

/** Verify NMEA checksum (*HH suffix). @thread_safe: yes (pure) */
bool nmea_verify_checksum(const char *sentence);

/** Identify sentence type (GGA/RMC/GSA). @thread_safe: yes (pure) */
nmea_sentence_t nmea_identify(const char *sentence);

/** Parse one sentence, updating relevant fields in data. @thread_safe: caller_must_lock */
nmea_err_t nmea_parse(const char *sentence, nmea_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* NMEA_PARSER_H */
