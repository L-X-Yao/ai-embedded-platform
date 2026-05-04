// SPDX-License-Identifier: MIT
// LibFuzzer harness for json_parser — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_json_parser.c ../json_parser.c -o fuzz_json_parser
// Run:     ./fuzz_json_parser -max_total_time=60 corpus_json_parser/

#include "../json_parser.h"
#include <stdint.h>
#include <stddef.h>

// No-op callback — just exercises the parser state machine
static void noop_cb(void *ctx, json_event_t event,
                    const char *value, uint16_t value_len) {
    (void)ctx;
    (void)event;
    (void)value;
    (void)value_len;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    json_parser_t parser;
    json_parser_init(&parser, noop_cb, NULL, 32);
    json_parser_feed(&parser, (const char *)data, len);
    json_parser_finish(&parser);

    return 0;
}
