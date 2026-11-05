// SPDX-License-Identifier: MIT
// LibFuzzer harness for rle_codec -- coverage-guided fuzzing
//
// Decodes the input as raw pairs into exact-size buffers of both the worst-case
// and a too-small size, then round-trips it through the encoder.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_rle_codec.c ../rle_codec.c -o fuzz_rle_codec
// Run:     ./fuzz_rle_codec -max_total_time=60 corpus_rle_codec/

#include "../rle_codec.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void decode_into(const uint8_t *src, size_t len, size_t cap) {
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    size_t out_len = 0;
    if (rle_codec_decode(src, len, out, cap, &out_len) == RLE_CODEC_OK && out_len > cap) {
        __builtin_trap();
    }
    free(out);
}

static void round_trip(const uint8_t *data, size_t size) {
    size_t cap = rle_codec_encode_bound(size);
    uint8_t *wire = malloc(cap ? cap : 1);
    uint8_t *out = malloc(size ? size : 1);

    if (wire && out) {
        size_t wire_len = 0;
        if (rle_codec_encode(data, size, wire, cap, &wire_len) != RLE_CODEC_OK ||
            wire_len > cap || wire_len % 2 != 0) {
            __builtin_trap();
        }

        size_t out_len = 0;
        if (rle_codec_decode(wire, wire_len, out, size, &out_len) != RLE_CODEC_OK ||
            out_len != size || (size > 0 && memcmp(out, data, size) != 0)) {
            __builtin_trap();
        }

        // Encoding into one pair less than needed has to be refused.
        if (wire_len >= 2 && rle_codec_encode(data, size, wire, wire_len - 2, &out_len) !=
                                 RLE_CODEC_ERR_OVERFLOW) {
            __builtin_trap();
        }
    }
    free(wire);
    free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    decode_into(data, size, rle_codec_decode_bound(size));
    decode_into(data, size, size > 0 ? data[0] : 0);
    round_trip(data, size);

    return 0;
}
