// SPDX-License-Identifier: MIT
// LibFuzzer harness for nmea_parser — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_nmea_parser.c ../nmea_parser.c -o fuzz_nmea_parser
// Run:     ./fuzz_nmea_parser -max_total_time=60 corpus_nmea_parser/

#include "../nmea_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

    return 0;
}
