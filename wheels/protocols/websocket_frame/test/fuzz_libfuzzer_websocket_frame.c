// SPDX-License-Identifier: MIT
// LibFuzzer harness for websocket_frame — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_websocket_frame.c ../websocket_frame.c -o fuzz_websocket_frame
// Run:     ./fuzz_websocket_frame -max_total_time=60 corpus_websocket_frame/

#include "../websocket_frame.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MASK_KEY_SIZE 4
#define MAX_ROUNDS 8

static const websocket_frame_opcode_t opcodes[] = {
    WEBSOCKET_FRAME_OPCODE_CONT,  WEBSOCKET_FRAME_OPCODE_TEXT, WEBSOCKET_FRAME_OPCODE_BINARY,
    WEBSOCKET_FRAME_OPCODE_CLOSE, WEBSOCKET_FRAME_OPCODE_PING, WEBSOCKET_FRAME_OPCODE_PONG,
};
#define OPCODE_COUNT (sizeof(opcodes) / sizeof(opcodes[0]))

static size_t frame_size(size_t payload_len, bool masked) {
    size_t header = 2;
    if (payload_len > 65535) {
        header = 10;
    } else if (payload_len > 125) {
        header = 4;
    }
    return header + (masked ? MASK_KEY_SIZE : 0) + payload_len;
}

// Lengths cluster on the edges of the 7-bit, 16-bit and 64-bit length fields.
static size_t pick_length(fuzz_input_t *in, uint8_t op) {
    size_t n = fuzz_u16(in);
    switch (op & 0x03) {
    case 0:  return n % 130;
    case 1:  return 65530 + n % 12;
    default: return n % 1024;
    }
}

static void check_frame(const uint8_t *wire, size_t need, bool fin,
                        websocket_frame_opcode_t opcode, const uint8_t *mask,
                        const uint8_t *payload, size_t len) {
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    if (websocket_frame_parse(wire, need, &frame) != WEBSOCKET_FRAME_OK ||
        frame.fin != fin || frame.opcode != opcode || frame.masked != (mask != NULL) ||
        frame.payload_len != len || frame.frame_len != need) {
        __builtin_trap();
    }
    if (mask && memcmp(frame.mask_key, mask, MASK_KEY_SIZE) != 0) __builtin_trap();
    if (len == 0) return;

    uint8_t *plain = malloc(len);
    if (!plain) return;
    memcpy(plain, frame.payload, len);
    if (mask) websocket_frame_mask_payload(plain, len, frame.mask_key);
    if (memcmp(plain, payload, len) != 0) __builtin_trap();
    free(plain);
}

static void round_trip(fuzz_input_t *in) {
    uint8_t op = fuzz_u8(in);
    websocket_frame_opcode_t opcode = opcodes[fuzz_u8(in) % OPCODE_COUNT];
    bool fin = (op & 0x04) != 0;
    uint8_t key[MASK_KEY_SIZE] = {0};
    fuzz_take(in, key, sizeof(key));
    const uint8_t *mask = (op & 0x08) ? key : NULL;
    size_t len = pick_length(in, op);

    uint8_t *payload = malloc(len ? len : 1);
    size_t need = frame_size(len, mask != NULL);
    uint8_t *wire = malloc(need);
    if (!payload || !wire) {
        free(payload);
        free(wire);
        return;
    }
    for (size_t i = 0; i < len; i++) payload[i] = (uint8_t)(i * 31 + key[0]);
    fuzz_take(in, payload, len);
    const uint8_t *src = len > 0 ? payload : NULL;

    size_t out_len = 0;
    if (websocket_frame_build(wire, need - 1, &out_len, fin, opcode, mask, src, len) ==
        WEBSOCKET_FRAME_OK) {
        __builtin_trap();
    }

    // RFC 6455 5.5: a control frame is one unfragmented frame of at most 125 bytes.
    bool control = ((uint8_t)opcode & 0x08) != 0;
    bool legal = !control || (fin && len <= 125);
    websocket_frame_err_t err =
        websocket_frame_build(wire, need, &out_len, fin, opcode, mask, src, len);
    if ((err == WEBSOCKET_FRAME_OK) != legal) __builtin_trap();
    if (legal) {
        if (out_len != need) __builtin_trap();
        check_frame(wire, need, fin, opcode, mask, payload, len);
    }
    free(wire);
    free(payload);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    websocket_frame_parse(data, size, &frame);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    for (int i = 0; i < MAX_ROUNDS && in.left > 0; i++) {
        round_trip(&in);
    }

    return 0;
}
