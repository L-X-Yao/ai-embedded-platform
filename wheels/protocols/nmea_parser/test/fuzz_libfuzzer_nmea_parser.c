// SPDX-License-Identifier: MIT
// LibFuzzer harness for nmea_parser — coverage-guided fuzzing
//
// Parses the input as raw text, then inside a sentence with a correct checksum,
// so the bytes reach the field parsers. Every stored value has to be in range.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_nmea_parser.c ../nmea_parser.c -o fuzz_nmea_parser
// Run:     ./fuzz_nmea_parser -max_total_time=60 corpus_nmea_parser/

#include "../nmea_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define WRAPPED_MAX 128

static void check_ranges(const nmea_parser_data_t *d) {
    // Written so that a NaN fails too.
    if (!(d->speed_kmh >= 0.0f && d->speed_kmh <= 1852.1f)) __builtin_trap();
    if (!(d->course_deg >= 0.0f && d->course_deg <= 360.0f)) __builtin_trap();
    if (!(d->altitude_m >= -1e6f && d->altitude_m <= 1e6f)) __builtin_trap();
    if (!(d->hdop >= 0.0f && d->hdop <= 1000.0f)) __builtin_trap();
    if (!(d->pdop >= 0.0f && d->pdop <= 1000.0f)) __builtin_trap();
    if (!(d->vdop >= 0.0f && d->vdop <= 1000.0f)) __builtin_trap();
    if (d->hour > 23 || d->min > 59 || d->sec > 60) __builtin_trap();
    if (d->month > 12 || d->day > 31) __builtin_trap();
    if (d->fix_quality > 9 || d->satellites > 99) __builtin_trap();
}

static void parse_with_good_sum(const uint8_t *data, size_t size) {
    static const char *const heads[] = { "$GPGGA,", "$GPRMC,", "$GPGSA," };
    static const char digits[] = "0123456789ABCDEF";
    if (size == 0) return;

    char line[8 + WRAPPED_MAX + 4];
    const char *head = heads[data[0] % 3];
    size_t len = strlen(head);
    memcpy(line, head, len);

    // NUL and '*' would end the part the sum covers.
    size_t n = size - 1 < WRAPPED_MAX ? size - 1 : WRAPPED_MAX;
    for (size_t i = 0; i < n; i++) {
        char c = (char)data[1 + i];
        line[len++] = (c == '\0' || c == '*') ? ',' : c;
    }

    uint8_t sum = 0;
    for (size_t i = 1; i < len; i++) sum ^= (uint8_t)line[i];
    line[len++] = '*';
    line[len++] = digits[sum >> 4];
    line[len++] = digits[sum & 0x0F];
    line[len] = '\0';

    nmea_parser_data_t gps;
    nmea_parser_data_init(&gps);
    if (nmea_parser_parse(line, &gps) != NMEA_PARSER_OK) __builtin_trap();
    check_ranges(&gps);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // nmea_parser_parse expects a NUL-terminated string.
    // Copy fuzz data into a stack buffer and NUL-terminate it.
    // Cap at 1024 to avoid stack overflow — NMEA sentences are short.
    size_t copy_len = size < 1024 ? size : 1024;
    char buf[1025];
    memcpy(buf, data, copy_len);
    buf[copy_len] = '\0';

    // Exercise all parse entry points
    nmea_parser_verify_checksum(buf);
    nmea_parser_identify(buf);

    nmea_parser_data_t gps;
    nmea_parser_data_init(&gps);
    nmea_parser_parse(buf, &gps);
    check_ranges(&gps);

    parse_with_good_sum(data, size);

    return 0;
}
