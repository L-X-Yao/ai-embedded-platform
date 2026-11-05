// SPDX-License-Identifier: MIT
// LibFuzzer harness for sbus -- coverage-guided fuzzing
//
// Feeds the input to the frame parser, then forces a valid header and footer so
// every 25-byte window reaches the decoder. Decoded frames have to encode back
// to the same channel and flag bits.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_sbus.c ../sbus.c -o fuzz_sbus
// Run:     ./fuzz_sbus -max_total_time=60 corpus_sbus/

#include "../sbus.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FLAG_MASK (SBUS_FLAG_CH17 | SBUS_FLAG_CH18 | SBUS_FLAG_FRAME_LOST | SBUS_FLAG_FAILSAFE)

static void check_frame(const uint8_t *wire) {
    sbus_frame_t frame;
    if (sbus_decode(wire, &frame) != SBUS_OK) return;

    for (int ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
        if (frame.channels[ch] > SBUS_CHANNEL_MAX) __builtin_trap();
    }

    uint8_t again[SBUS_FRAME_LEN];
    if (sbus_encode(&frame, again) != SBUS_OK) __builtin_trap();
    if (memcmp(&wire[1], &again[1], 22) != 0) __builtin_trap();
    if ((wire[23] & FLAG_MASK) != again[23]) __builtin_trap();
}

static void check_conversions(uint16_t value) {
    uint16_t us = sbus_channel_to_us(value);
    if (us < SBUS_US_MIN || us > SBUS_US_MAX) __builtin_trap();

    uint16_t channel = sbus_us_to_channel(value);
    if (channel < SBUS_RC_MIN || channel > SBUS_RC_MAX) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    sbus_parser_t parser;
    sbus_parser_init(&parser);

    for (size_t i = 0; i < size; i++) {
        if (sbus_parse_byte(&parser, data[i]) == SBUS_PARSE_COMPLETE) {
            check_frame(sbus_parser_get_buf(&parser));
        }
    }

    for (size_t off = 0; off + SBUS_FRAME_LEN <= size; off += SBUS_FRAME_LEN) {
        uint8_t wire[SBUS_FRAME_LEN];
        memcpy(wire, &data[off], sizeof(wire));
        check_frame(wire);

        wire[0] = SBUS_HEADER;
        wire[SBUS_FRAME_LEN - 1] = SBUS_FOOTER;
        check_frame(wire);
    }

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        check_conversions(fuzz_u16(&in));
    }

    return 0;
}
