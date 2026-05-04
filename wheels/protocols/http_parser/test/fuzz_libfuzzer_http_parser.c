// SPDX-License-Identifier: MIT
// LibFuzzer harness for http_parser — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_http_parser.c ../http_parser.c -o fuzz_http_parser
// Run:     ./fuzz_http_parser -max_total_time=60 corpus_http_parser/

#include "../http_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // http_parser takes const char*, cast from uint8_t*
    const char *text = (const char *)data;

    // Parse as HTTP request
    http_parser_request_t req;
    memset(&req, 0, sizeof(req));
    http_parser_parse_request(text, size, &req);

    // Parse as HTTP response
    http_parser_response_t resp;
    memset(&resp, 0, sizeof(resp));
    http_parser_parse_response(text, size, &resp);

    return 0;
}
