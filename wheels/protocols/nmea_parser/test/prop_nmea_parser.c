// SPDX-License-Identifier: MIT
/*
 * prop_nmea_parser.c — property-based tests for nmea_parser
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_nmea_parser.c ../nmea_parser.c -o prop_nmea_parser
 */

#include "test_harness.h"
#include "property_test.h"
#include "../nmea_parser.h"
#include <string.h>
#include <stdio.h>

/* Helper: compute XOR checksum of characters between $ and * */
static uint8_t nmea_checksum(const char *body, int len)
{
    uint8_t cs = 0;
    for (int i = 0; i < len; i++)
        cs ^= (uint8_t)body[i];
    return cs;
}

/* Property: parse synthetically valid GGA sentence -> fix_quality + sats populated */
PROPERTY(gga_parse_valid, 10000) {
    /* Generate random but plausible GGA fields */
    uint8_t hour = (uint8_t)ARBI_RANGE(seed, 0, 23);
    uint8_t min  = (uint8_t)ARBI_RANGE(seed, 0, 59);
    uint8_t sec  = (uint8_t)ARBI_RANGE(seed, 0, 59);
    int lat_deg  = ARBI_RANGE(seed, 0, 89);
    int lat_min  = ARBI_RANGE(seed, 0, 59);
    int lon_deg  = ARBI_RANGE(seed, 0, 179);
    int lon_min  = ARBI_RANGE(seed, 0, 59);
    uint8_t fix  = (uint8_t)ARBI_RANGE(seed, 1, 2);
    uint8_t sats = (uint8_t)ARBI_RANGE(seed, 3, 12);

    char body[256];
    int blen = snprintf(body, sizeof(body),
        "GPGGA,%02u%02u%02u,%02d%02d.0000,N,%03d%02d.0000,E,%u,%02u,1.0,100.0,M,0.0,M,,",
        hour, min, sec, lat_deg, lat_min, lon_deg, lon_min,
        (unsigned)fix, (unsigned)sats);
    PROP_ASSERT(blen > 0 && blen < (int)sizeof(body));

    uint8_t cs = nmea_checksum(body, blen);
    char sentence[300];
    int slen = snprintf(sentence, sizeof(sentence), "$%s*%02X", body, cs);
    PROP_ASSERT(slen > 0);

    /* Verify our checksum is correct */
    PROP_ASSERT(nmea_parser_verify_checksum(sentence));

    nmea_parser_sentence_t stype = nmea_parser_identify(sentence);
    PROP_ASSERT_EQ(stype, NMEA_PARSER_SENTENCE_GGA);

    nmea_parser_data_t data;
    nmea_parser_data_init(&data);
    nmea_parser_err_t err = nmea_parser_parse(sentence, &data);
    PROP_ASSERT_EQ(err, NMEA_PARSER_OK);

    /* GGA parser sets fix_quality and satellites (not time — that's RMC) */
    PROP_ASSERT_EQ(data.fix_quality, fix);
    PROP_ASSERT_EQ(data.satellites, sats);
}

/* Property: parse is deterministic */
PROPERTY(parse_determinism, 5000) {
    (void)seed;
    const char *sentence = "$GPRMC,120000,A,4807.038,N,01131.000,E,0.0,0.0,010120,,,A*";
    /* Build with valid checksum */
    const char *body = sentence + 1;
    const char *star = strchr(body, '*');
    PROP_ASSERT(star != NULL);
    uint8_t cs = nmea_checksum(body, (int)(star - body));
    char full[256];
    snprintf(full, sizeof(full), "%.*s*%02X", (int)(star - sentence), sentence, cs);

    nmea_parser_data_t d1, d2;
    nmea_parser_data_init(&d1);
    nmea_parser_data_init(&d2);
    nmea_parser_err_t e1 = nmea_parser_parse(full, &d1);
    nmea_parser_err_t e2 = nmea_parser_parse(full, &d2);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(d1.fix_valid, d2.fix_valid);
    /* Compare the full struct (all bytes) */
    PROP_ASSERT_MEM_EQ(&d1, &d2, sizeof(d1));
}

/* Property: checksum verification catches single-byte corruption */
PROPERTY(checksum_detects_corruption, 10000) {
    char body[] = "GPGGA,120000,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,";
    uint8_t cs = nmea_checksum(body, (int)strlen(body));
    char sentence[256];
    snprintf(sentence, sizeof(sentence), "$%s*%02X", body, cs);

    /* Should pass initially */
    PROP_ASSERT(nmea_parser_verify_checksum(sentence));

    /* Corrupt one byte in body (after $ before *) */
    int corrupt_pos = 1 + (int)ARBI_RANGE(seed, 0, (int)strlen(body) - 1);
    uint8_t orig = (uint8_t)sentence[corrupt_pos];
    uint8_t flip = orig ^ (uint8_t)(1u << (ARBI_UINT8(seed) % 8u));
    if (flip == orig) flip ^= 1u; /* ensure at least 1 bit changed */
    sentence[corrupt_pos] = (char)flip;

    /* Should fail now */
    PROP_ASSERT(!nmea_parser_verify_checksum(sentence));
}

/* Property: parse of arbitrary string never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    char garbage[256];
    size_t len = ARBI_SIZE(seed, 255);
    ARBI_FILL(seed, (uint8_t *)garbage, len);
    garbage[len] = '\0'; /* NUL-terminate for parser */

    nmea_parser_data_t data;
    nmea_parser_data_init(&data);
    nmea_parser_parse(garbage, &data);
    nmea_parser_verify_checksum(garbage);
    nmea_parser_identify(garbage);
}

TEST_MAIN("nmea_parser properties") {
    RUN(gga_parse_valid);
    RUN(parse_determinism);
    RUN(checksum_detects_corruption);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
