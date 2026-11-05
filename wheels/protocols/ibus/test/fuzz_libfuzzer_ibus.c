// SPDX-License-Identifier: MIT
// LibFuzzer harness for ibus -- coverage-guided fuzzing
//
// Decodes the input as raw bytes, then writes the header and checksum so channel
// data always reaches the decoder. Decoded frames have to encode back unchanged.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_ibus.c ../ibus.c -o fuzz_ibus
// Run:     ./fuzz_ibus -max_total_time=60 corpus_ibus/

#include "../ibus.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CHANNEL_BYTES (IBUS_FRAME_LEN - 4)

static void check_wrapped(const uint8_t *data, size_t size, uint8_t *wire, uint8_t *again) {
    ibus_frame_t frame;

    wire[0] = 0x20;
    wire[1] = 0x40;
    memcpy(&wire[2], data, size < CHANNEL_BYTES ? size : CHANNEL_BYTES);
    uint16_t sum = ibus_checksum(wire, IBUS_FRAME_LEN - 2);
    wire[IBUS_FRAME_LEN - 2] = (uint8_t)(sum & 0xFF);
    wire[IBUS_FRAME_LEN - 1] = (uint8_t)(sum >> 8);

    if (ibus_decode(wire, IBUS_FRAME_LEN, &frame) != IBUS_OK) __builtin_trap();

    if (ibus_encode(&frame, again, IBUS_FRAME_LEN - 1) != IBUS_ERR_SHORT) __builtin_trap();
    if (ibus_encode(&frame, again, IBUS_FRAME_LEN) != IBUS_OK) __builtin_trap();
    if (memcmp(wire, again, IBUS_FRAME_LEN) != 0) __builtin_trap();

    // A flipped channel bit moves the sum, so the checksum has to catch it.
    if (size > CHANNEL_BYTES) {
        uint8_t pick = data[CHANNEL_BYTES];
        wire[2 + pick % CHANNEL_BYTES] ^= (uint8_t)(1u << (pick >> 5));
        if (ibus_decode(wire, IBUS_FRAME_LEN, &frame) != IBUS_ERR_CHECKSUM) __builtin_trap();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ibus_frame_t frame;
    ibus_decode(data, size, &frame);

    // Exact-size frames, so ASan sees any access past the checksum.
    uint8_t *wire = calloc(1, IBUS_FRAME_LEN);
    uint8_t *again = calloc(1, IBUS_FRAME_LEN);
    if (wire && again) check_wrapped(data, size, wire, again);
    free(wire);
    free(again);

    return 0;
}
