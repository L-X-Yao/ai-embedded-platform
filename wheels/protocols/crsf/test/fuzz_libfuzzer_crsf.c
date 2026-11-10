// SPDX-License-Identifier: MIT
// LibFuzzer harness for crsf -- coverage-guided fuzzing
//
// Decodes the input as raw bytes, then wraps it in frames with a correct length
// byte and CRC. The encoder gets arbitrary length fields, and channel payloads
// have to survive unpack and pack unchanged.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_crsf.c ../crsf.c -o fuzz_crsf
// Run:     ./fuzz_crsf -max_total_time=60 corpus_crsf/

#include "../crsf.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void check_channels(const uint8_t *payload, size_t len) {
    crsf_channels_t channels;
    if (crsf_unpack_channels(payload, len, &channels) != CRSF_OK) return;

    uint8_t packed[CRSF_CHANNEL_BYTES];
    if (crsf_pack_channels(&channels, packed, sizeof(packed)) != CRSF_OK) __builtin_trap();
    if (memcmp(packed, payload, CRSF_CHANNEL_BYTES) != 0) __builtin_trap();

    for (int ch = 0; ch < CRSF_NUM_CHANNELS; ch++) {
        if (channels.channels[ch] > 0x7FF) __builtin_trap();
    }
}

static void decode_wrapped(fuzz_input_t *in) {
    uint8_t addr = fuzz_u8(in);
    uint8_t type = fuzz_u8(in);
    size_t payload_len = fuzz_u8(in) % (CRSF_MAX_FRAME_SIZE + 9);

    // Exact-size frame, so ASan sees any read past the CRC.
    size_t total = payload_len + 4;
    uint8_t *wire = calloc(1, total);
    if (!wire) return;
    wire[0] = addr;
    wire[1] = (uint8_t)(payload_len + 2);
    wire[2] = type;
    fuzz_take(in, &wire[3], payload_len);
    wire[3 + payload_len] = crsf_crc8(&wire[2], payload_len + 1);

    crsf_frame_t frame;
    crsf_err_t err = crsf_decode_frame(wire, total, &frame);
    if (payload_len > CRSF_MAX_PAYLOAD) {
        if (err != CRSF_ERR_FORMAT) __builtin_trap();
    } else {
        if (err != CRSF_OK || frame.addr != addr || frame.type != type ||
            frame.payload_len != payload_len ||
            memcmp(frame.payload, &wire[3], payload_len) != 0) {
            __builtin_trap();
        }
        check_channels(frame.payload, frame.payload_len);
    }
    free(wire);
}

static void encode_any_length(fuzz_input_t *in) {
    crsf_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.addr = fuzz_u8(in);
    frame.type = fuzz_u8(in);
    frame.payload_len = fuzz_u8(in);
    fuzz_take(in, frame.payload, sizeof(frame.payload));

    // Exact-size output, so ASan sees any write past cap.
    size_t cap = fuzz_u8(in);
    uint8_t *buf = malloc(cap ? cap : 1);
    if (!buf) return;

    size_t out_len = 0;
    if (crsf_encode_frame(&frame, buf, cap, &out_len) == CRSF_OK) {
        crsf_frame_t back;
        if (crsf_decode_frame(buf, out_len, &back) != CRSF_OK ||
            back.payload_len != frame.payload_len ||
            memcmp(back.payload, frame.payload, frame.payload_len) != 0) {
            __builtin_trap();
        }
    }
    free(buf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    crsf_frame_t frame;
    if (crsf_decode_frame(data, size, &frame) == CRSF_OK) {
        check_channels(frame.payload, frame.payload_len);
    }
    check_channels(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        decode_wrapped(&in);
        encode_any_length(&in);
    }

    return 0;
}
