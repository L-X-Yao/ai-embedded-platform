// SPDX-License-Identifier: MIT
// LibFuzzer harness for utf8 -- coverage-guided fuzzing
//
// Walks the input codepoint by codepoint. Every decoded codepoint has to encode
// back to the same bytes, and validate and count have to agree with the walk.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_utf8.c ../utf8.c -o fuzz_utf8
// Run:     ./fuzz_utf8 -max_total_time=60 corpus_utf8/

#include "../utf8.h"
#include "fuzz_input.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void check_codepoint(uint32_t codepoint, const uint8_t *bytes, size_t len) {
    uint8_t *again = malloc(len);
    if (!again) return;

    size_t out_len = 0;
    if (utf8_encode(codepoint, again, len - 1, &out_len) != UTF8_ERR_SHORT) __builtin_trap();
    if (utf8_encode(codepoint, again, len, &out_len) != UTF8_OK || out_len != len ||
        memcmp(again, bytes, len) != 0) {
        __builtin_trap();
    }
    free(again);
}

static void walk(const uint8_t *data, size_t size) {
    size_t pos = 0;
    int32_t count = 0;
    bool valid = true;

    while (pos < size) {
        uint32_t codepoint = 0;
        size_t len = 0;
        if (utf8_decode(&data[pos], size - pos, &codepoint, &len) != UTF8_OK) {
            valid = false;
            break;
        }
        if (len == 0 || len > 4 || len != utf8_byte_length(data[pos])) __builtin_trap();
        check_codepoint(codepoint, &data[pos], len);
        pos += len;
        count++;
    }

    if ((utf8_validate(data, size) == UTF8_OK) != valid) __builtin_trap();
    if (utf8_codepoint_count(data, size) != (valid ? count : -1)) __builtin_trap();
}

// Any 32-bit value: what encodes has to decode back to the same codepoint.
static void check_encoder(uint32_t codepoint) {
    uint8_t buf[4];
    size_t len = 0;
    if (utf8_encode(codepoint, buf, sizeof(buf), &len) != UTF8_OK) {
        if (codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            __builtin_trap();
        }
        return;
    }

    uint32_t back = 0;
    size_t used = 0;
    if (utf8_decode(buf, len, &back, &used) != UTF8_OK || back != codepoint || used != len) {
        __builtin_trap();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    walk(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        uint32_t hi = fuzz_u16(&in);
        check_encoder((hi << 16) | fuzz_u16(&in));
        check_encoder(((hi & 0x1F) << 16) | fuzz_u16(&in));
    }

    return 0;
}
