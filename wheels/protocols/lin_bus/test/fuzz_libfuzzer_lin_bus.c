// SPDX-License-Identifier: MIT
// LibFuzzer harness for lin_bus -- coverage-guided fuzzing
//
// Decodes the input as raw bytes, then encodes frames built from the input so
// they get past the PID parity and the checksum.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_lin_bus.c ../lin_bus.c -o fuzz_lin_bus
// Run:     ./fuzz_lin_bus -max_total_time=60 corpus_lin_bus/

#include "../lin_bus.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// A single flipped PID bit always breaks parity: every ID bit feeds P0 or P1.
static void check_pid(uint8_t id, uint8_t bit) {
    uint8_t pid = lin_bus_calc_pid(id);
    if (!lin_bus_verify_pid(pid)) __builtin_trap();
    if (lin_bus_verify_pid((uint8_t)(pid ^ (1u << bit)))) __builtin_trap();
}

static void decode_back(const uint8_t *wire, size_t len, const lin_bus_frame_t *frame,
                        uint8_t bit) {
    lin_bus_frame_t back;
    if (lin_bus_decode(wire, len, &back) != LIN_BUS_OK || back.id != frame->id ||
        back.data_len != frame->data_len ||
        memcmp(back.data, frame->data, frame->data_len) != 0) {
        __builtin_trap();
    }

    // Exact-size copy with the PID corrupted, so ASan still sees any over-read.
    uint8_t *bad = malloc(len);
    if (!bad) return;
    memcpy(bad, wire, len);
    bad[1] ^= (uint8_t)(1u << bit);
    if (lin_bus_decode(bad, len, &back) != LIN_BUS_ERR_PARITY) __builtin_trap();
    free(bad);
}

// Each record is [op][id][data]:
//   op bits 0-2: data length minus one
//   op bit 3:    keep all eight id bits, which the encoder has to refuse above 63
//   op bit 4:    enhanced checksum instead of classic
//   op bits 5-7: bit to flip in the PID
static void round_trip(fuzz_input_t *in) {
    while (in->left > 0) {
        uint8_t op = fuzz_u8(in);
        uint8_t id = fuzz_u8(in);
        uint8_t bit = (uint8_t)(op >> 5);

        lin_bus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.id = (op & 0x08) ? id : (uint8_t)(id & LIN_BUS_MAX_ID);
        frame.data_len = (uint8_t)((op & 0x07) + 1);
        frame.checksum_type = (op & 0x10) ? LIN_BUS_CHECKSUM_ENHANCED
                                          : LIN_BUS_CHECKSUM_CLASSIC;
        fuzz_take(in, frame.data, frame.data_len);

        // Exact-size frame, so ASan sees any write past the end.
        size_t need = (size_t)frame.data_len + 3;
        uint8_t *wire = malloc(need);
        if (!wire) return;

        size_t len = 0;
        if (lin_bus_encode(&frame, wire, need - 1, &len) == LIN_BUS_OK) __builtin_trap();

        lin_bus_err_t err = lin_bus_encode(&frame, wire, need, &len);
        if (frame.id > LIN_BUS_MAX_ID) {
            if (err != LIN_BUS_ERR_PARAM) __builtin_trap();
        } else {
            if (err != LIN_BUS_OK || len != need) __builtin_trap();
            decode_back(wire, len, &frame, bit);
            check_pid(frame.id, bit);
        }
        free(wire);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lin_bus_frame_t frame;
    lin_bus_decode(data, size, &frame);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
