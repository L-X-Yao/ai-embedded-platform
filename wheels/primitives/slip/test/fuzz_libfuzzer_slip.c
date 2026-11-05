// SPDX-License-Identifier: MIT
// LibFuzzer harness for slip -- coverage-guided fuzzing
//
// Decodes the input with the block and streaming decoders, round-trips it through
// the encoder, and encodes worst-case lengths near the uint16_t limit.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_slip.c ../slip.c -o fuzz_slip
// Run:     ./fuzz_slip -max_total_time=60 corpus_slip/

#include "../slip.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LONG_START 32767u
#define LONG_CAP   65535u

static uint8_t g_long_input[65535];

static void decode_block(const uint8_t *data, uint16_t len) {
    // Exact-size output, so ASan sees any write past the claimed capacity.
    uint16_t cap = len > 0 ? data[0] : 0;
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    uint16_t out_len = cap;
    if (slip_decode(data, len, out, &out_len) == SLIP_OK && out_len > cap) __builtin_trap();
    free(out);
}

static void decode_stream(const uint8_t *data, size_t size) {
    if (size == 0) return;

    uint16_t capacity = (uint16_t)(data[0] + 1u);
    uint8_t *buf = malloc(capacity);
    if (!buf) return;

    slip_decoder_t dec;
    if (slip_decoder_init(&dec, buf, capacity) != SLIP_OK) __builtin_trap();

    for (size_t i = 1; i < size; i++) {
        slip_decoder_feed(&dec, data[i]);
        if (slip_decoder_ready(&dec)) {
            const uint8_t *frame = NULL;
            uint16_t frame_len = slip_decoder_frame(&dec, &frame);
            if (frame_len > capacity || (frame_len > 0 && !frame)) __builtin_trap();
            slip_decoder_reset(&dec);
        }
    }
    free(buf);
}

static void round_trip(const uint8_t *data, uint16_t len) {
    uint16_t need = slip_max_encoded_size(len);
    uint8_t *wire = malloc(need);
    uint8_t *out = malloc(len ? len : 1);

    if (wire && out) {
        uint16_t wire_len = need;
        if (slip_encode(data, len, wire, &wire_len) != SLIP_OK || wire_len > need) {
            __builtin_trap();
        }

        uint16_t out_len = len;
        if (slip_decode(wire, wire_len, out, &out_len) != SLIP_OK || out_len != len ||
            (len > 0 && memcmp(out, data, len) != 0)) {
            __builtin_trap();
        }

        // A non-empty frame through the streaming decoder has to match as well.
        if (len > 0) {
            slip_decoder_t dec;
            if (slip_decoder_init(&dec, out, len) != SLIP_OK) __builtin_trap();
            for (uint16_t i = 0; i < wire_len; i++) slip_decoder_feed(&dec, wire[i]);

            const uint8_t *frame = NULL;
            if (!slip_decoder_ready(&dec) || slip_decoder_frame(&dec, &frame) != len ||
                memcmp(frame, data, len) != 0) {
                __builtin_trap();
            }
        }
    }
    free(wire);
    free(out);
}

// Data of END bytes doubles in size, so a length past 32766 cannot fit any
// 16-bit capacity and has to be refused.
static void encode_long(uint8_t pick) {
    uint8_t *out = malloc(LONG_CAP);
    if (!out) return;

    memset(g_long_input, SLIP_END, sizeof(g_long_input));
    uint16_t len = (uint16_t)(LONG_START + (uint32_t)pick * 128u);
    uint16_t out_len = LONG_CAP;
    if (slip_encode(g_long_input, len, out, &out_len) != SLIP_ERR_OVERFLOW) __builtin_trap();
    free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0x7FFE ? 0x7FFE : size);

    decode_block(data, len);
    decode_stream(data, size);
    round_trip(data, len);
    if (size > 0 && (data[0] & 0x0F) == 0x0F) encode_long(data[size - 1]);

    return 0;
}
