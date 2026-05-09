// SPDX-License-Identifier: MIT
// LibFuzzer harness for ncp5623_parser -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_ncp5623_parser.c ../ncp5623_parser.c -o fuzz_ncp5623_parser
// Run:     ./fuzz_ncp5623_parser -max_total_time=60 corpus_ncp5623_parser/

#include "../ncp5623_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ncp5623_parser_t parser;
    memset(&parser, 0, sizeof(parser));
    ncp5623_parser_init(&parser);

    // Simulate I2C transactions: start, feed bytes, stop
    // Split fuzz input into multiple simulated I2C transactions
    ncp5623_parser_start(&parser);
    for (size_t i = 0; i < size; i++) {
        ncp5623_evt_t evt;
        memset(&evt, 0, sizeof(evt));
        ncp5623_parser_feed(&parser, data[i], &evt);

        // Periodically simulate transaction boundaries
        if (data[i] == 0xFF && i + 1 < size) {
            ncp5623_parser_stop(&parser);
            ncp5623_parser_start(&parser);
        }
    }
    ncp5623_parser_stop(&parser);

    // Second pass: feed bytes one per transaction (worst case for parser)
    ncp5623_parser_init(&parser);
    for (size_t i = 0; i < size && i < 256; i++) {
        ncp5623_parser_start(&parser);
        ncp5623_evt_t evt;
        memset(&evt, 0, sizeof(evt));
        ncp5623_parser_feed(&parser, data[i], &evt);
        ncp5623_parser_stop(&parser);
    }

    return 0;
}
