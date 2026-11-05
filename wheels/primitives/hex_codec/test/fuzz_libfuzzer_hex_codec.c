// SPDX-License-Identifier: MIT
// LibFuzzer harness for hex_codec -- coverage-guided fuzzing
//
// Decodes the input as raw text into an exact-size buffer, then round-trips it
// through the lower and upper case encoders.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_hex_codec.c ../hex_codec.c -o fuzz_hex_codec
// Run:     ./fuzz_hex_codec -max_total_time=60 corpus_hex_codec/

#include "../hex_codec.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void decode_raw(const uint8_t *data, uint16_t len) {
    uint16_t cap = hex_decoded_size(len);
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    uint16_t out_len = cap;
    if (hex_decode((const char *)data, len, out, &out_len) == HEX_OK && out_len > cap) {
        __builtin_trap();
    }

    // One byte short has to be refused, not written past the end.
    if (cap > 0) {
        out_len = (uint16_t)(cap - 1u);
        hex_err_t err = hex_decode((const char *)data, len, out, &out_len);
        if (err == HEX_OK) __builtin_trap();
    }
    free(out);
}

typedef hex_err_t (*encode_fn)(const uint8_t *, uint16_t, char *, uint16_t);

static void round_trip(encode_fn encode, const uint8_t *data, uint16_t len) {
    uint16_t need = (uint16_t)(hex_encoded_size(len) + 1u);
    char *text = malloc(need);
    uint8_t *out = malloc(len ? len : 1);

    if (text && out) {
        if (encode(data, len, text, (uint16_t)(need - 1u)) != HEX_ERR_OVERFLOW) __builtin_trap();
        if (encode(data, len, text, need) != HEX_OK) __builtin_trap();

        uint16_t out_len = len;
        if (hex_decode(text, (uint16_t)(need - 1u), out, &out_len) != HEX_OK || out_len != len ||
            (len > 0 && memcmp(out, data, len) != 0)) {
            __builtin_trap();
        }
    }
    free(text);
    free(out);
}

static void check_nibbles(uint8_t value) {
    int nibble = hex_char_to_nibble((char)value);
    if (nibble > 15) __builtin_trap();

    char lower = hex_nibble_to_char(value, false);
    char upper = hex_nibble_to_char(value, true);
    if (hex_char_to_nibble(lower) != (value & 0x0F)) __builtin_trap();
    if (hex_char_to_nibble(upper) != (value & 0x0F)) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0x7FFF ? 0x7FFF : size);

    decode_raw(data, len);
    round_trip(hex_encode, data, len);
    round_trip(hex_encode_upper, data, len);

    for (size_t i = 0; i < size && i < 256; i++) check_nibbles(data[i]);

    return 0;
}
