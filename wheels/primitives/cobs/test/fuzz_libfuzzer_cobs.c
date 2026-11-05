// SPDX-License-Identifier: MIT
// LibFuzzer harness for cobs -- coverage-guided fuzzing
//
// Decodes the input as raw bytes, round-trips it through the encoder, and encodes
// lengths near the uint16_t limit into the largest 16-bit capacity.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_cobs.c ../cobs.c -o fuzz_cobs
// Run:     ./fuzz_cobs -max_total_time=60 corpus_cobs/

#include "../cobs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LONG_START 65280u
#define LONG_CAP   65535u

static uint8_t g_long_input[65535];

static void decode_raw(const uint8_t *data, uint16_t len) {
    // Exact-size output, so ASan sees any write past the claimed capacity.
    uint16_t cap = len > 0 ? data[0] : 0;
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    uint16_t out_len = cap;
    if (cobs_decode(data, len, out, &out_len) == COBS_OK && out_len > cap) __builtin_trap();
    free(out);
}

static void round_trip(const uint8_t *data, uint16_t len) {
    uint16_t need = cobs_max_encoded_size(len);
    uint8_t *wire = malloc(need);
    uint8_t *out = malloc(len ? len : 1);

    if (wire && out) {
        uint16_t wire_len = need;
        if (cobs_encode(data, len, wire, &wire_len) != COBS_OK || wire_len > need) {
            __builtin_trap();
        }
        if (memchr(wire, 0, wire_len)) __builtin_trap();

        uint16_t out_len = len;
        if (cobs_decode(wire, wire_len, out, &out_len) != COBS_OK || out_len != len ||
            (len > 0 && memcmp(out, data, len) != 0)) {
            __builtin_trap();
        }
    }
    free(wire);
    free(out);
}

// Non-zero data past 65279 bytes needs more than 65535 encoded bytes, so the
// largest capacity a uint16_t can state still has to be refused.
static void encode_long(uint8_t pick) {
    uint8_t *out = malloc(LONG_CAP);
    if (!out) return;

    memset(g_long_input, 0x55, sizeof(g_long_input));
    uint16_t len = (uint16_t)(LONG_START + pick);
    uint16_t out_len = LONG_CAP;
    if (cobs_encode(g_long_input, len, out, &out_len) != COBS_ERR_OVERFLOW) __builtin_trap();
    free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0xFEFF ? 0xFEFF : size);

    decode_raw(data, len);
    round_trip(data, len);
    if (size > 0 && (data[0] & 0x0F) == 0x0F) encode_long(data[size - 1]);

    return 0;
}
