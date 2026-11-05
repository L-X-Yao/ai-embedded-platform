// SPDX-License-Identifier: MIT
// LibFuzzer harness for base64 -- coverage-guided fuzzing
//
// Decodes the input as raw text, round-trips it through the encoder, and encodes
// lengths near the uint16_t limit into a small buffer, which has to be refused.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_base64.c ../base64.c -o fuzz_base64
// Run:     ./fuzz_base64 -max_total_time=60 corpus_base64/

#include "../base64.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LONGEST_INPUT 49149u
#define SMALL_CAP     16u

static uint8_t g_long_input[65535];

static void decode_raw(const uint8_t *data, uint16_t len) {
    // Exact-size output, so ASan sees any write past the claimed capacity.
    uint16_t cap = (uint16_t)(base64_decoded_size(len) + 2u);
    uint8_t *out = malloc(cap);
    if (!out) return;

    uint16_t out_len = cap;
    if (base64_decode((const char *)data, len, out, &out_len) == BASE64_OK && out_len > cap) {
        __builtin_trap();
    }
    free(out);
}

static void round_trip(const uint8_t *data, uint16_t len) {
    uint16_t need = base64_encoded_size(len);
    char *text = malloc((size_t)need + 1);
    uint8_t *out = malloc(len ? len : 1);

    if (text && out) {
        uint16_t text_len = need;
        if (base64_encode(data, len, text, &text_len) != BASE64_ERR_OVERFLOW) __builtin_trap();

        text_len = (uint16_t)(need + 1u);
        if (base64_encode(data, len, text, &text_len) != BASE64_OK || text_len != need) {
            __builtin_trap();
        }

        uint16_t out_len = len;
        if (base64_decode(text, text_len, out, &out_len) != BASE64_OK || out_len != len ||
            (len > 0 && memcmp(out, data, len) != 0)) {
            __builtin_trap();
        }
    }
    free(text);
    free(out);
}

// The size math runs in uint16_t. A real length past 49149 needs more than
// 65535 characters, so no 16-bit capacity can hold it.
static void encode_long(uint8_t pick) {
    char *out = malloc(SMALL_CAP);
    if (!out) return;

    uint16_t len = (uint16_t)(LONGEST_INPUT + 1u + (uint32_t)pick * 64u);
    uint16_t out_len = SMALL_CAP;
    if (base64_encode(g_long_input, len, out, &out_len) != BASE64_ERR_OVERFLOW) {
        __builtin_trap();
    }
    free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);

    decode_raw(data, len);

    if (size <= LONGEST_INPUT) round_trip(data, len);
    if (size > 0) encode_long(data[0]);

    return 0;
}
