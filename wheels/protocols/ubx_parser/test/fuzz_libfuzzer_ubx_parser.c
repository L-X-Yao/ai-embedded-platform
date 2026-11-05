// SPDX-License-Identifier: MIT
// LibFuzzer harness for ubx_parser -- coverage-guided fuzzing
//
// Feeds the input to the parser as raw bytes, then builds frames from the input
// so messages get past the Fletcher checksum.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_ubx_parser.c ../ubx_parser.c -o fuzz_ubx_parser
// Run:     ./fuzz_ubx_parser -max_total_time=60 corpus_ubx_parser/

#include "../ubx_parser.h"
#include "fuzz_input.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_MAX (UBX_PARSER_MAX_PAYLOAD + UBX_OVERHEAD)

static void feed_raw(const uint8_t *data, size_t size) {
    if (size == 0) return;

    // Exact-size parse buffer. The first byte picks its size, so the length
    // checks also run against buffers too small for the frame.
    size_t buf_size = (data[0] & 1) ? FRAME_MAX : data[0];
    uint8_t *buf = malloc(buf_size ? buf_size : 1);
    if (!buf) return;

    ubx_parser_t parser;
    if (ubx_parser_init(&parser, buf, buf_size) == UBX_PARSER_OK) {
        for (size_t i = 1; i < size; i++) {
            ubx_feed_status_t status = ubx_parser_feed(&parser, data[i]);
            if (status == UBX_FEED_COMPLETE) {
                ubx_msg_t msg;
                if (ubx_parser_get_msg(&parser, &msg) != UBX_PARSER_OK) __builtin_trap();
                ubx_parser_reset(&parser);
            } else if (status == UBX_FEED_ERROR) {
                ubx_parser_reset(&parser);
            }
        }
    }
    free(buf);
}

static void check_message(const ubx_parser_t *parser, uint8_t msg_class, uint8_t msg_id,
                          const uint8_t *payload, uint16_t payload_len) {
    ubx_msg_t msg;
    if (ubx_parser_get_msg(parser, &msg) != UBX_PARSER_OK) __builtin_trap();
    if (msg.msg_class != msg_class || msg.msg_id != msg_id ||
        msg.payload_len != payload_len) {
        __builtin_trap();
    }
    if (payload_len > 0 && memcmp(msg.payload, payload, payload_len) != 0) {
        __builtin_trap();
    }
}

// Each record is [op][class][id][length][payload]:
//   op bit 0: flip one payload bit, which the checksum has to catch
static void round_trip(fuzz_input_t *in) {
    uint8_t buf[FRAME_MAX];
    ubx_parser_t parser;
    if (ubx_parser_init(&parser, buf, sizeof(buf)) != UBX_PARSER_OK) __builtin_trap();

    while (in->left > 0) {
        uint8_t op = fuzz_u8(in);
        uint8_t msg_class = fuzz_u8(in);
        uint8_t msg_id = fuzz_u8(in);
        uint8_t payload[UBX_PARSER_MAX_PAYLOAD];
        uint8_t wanted = fuzz_u8(in);
        uint16_t payload_len = (uint16_t)fuzz_take(in, payload, wanted);

        // Exact-size frame, so ASan sees any write past the end.
        size_t total = (size_t)payload_len + UBX_OVERHEAD;
        uint8_t *frame = malloc(total);
        if (!frame) return;

        if (ubx_parser_build(frame, total - 1, msg_class, msg_id, payload, payload_len) >= 0) {
            __builtin_trap();
        }
        if (ubx_parser_build(frame, total, msg_class, msg_id, payload, payload_len) !=
            (int)total) {
            __builtin_trap();
        }

        bool flip = (op & 0x01) && payload_len > 0;
        if (flip) {
            frame[UBX_HEADER_LEN + fuzz_u8(in) % payload_len] ^= (uint8_t)(1u << (op >> 5));
        }

        ubx_feed_status_t status = UBX_FEED_IDLE;
        for (size_t i = 0; i < total; i++) {
            status = ubx_parser_feed(&parser, frame[i]);
        }
        free(frame);

        if (status != (flip ? UBX_FEED_ERROR : UBX_FEED_COMPLETE)) __builtin_trap();
        if (!flip) check_message(&parser, msg_class, msg_id, payload, payload_len);
        ubx_parser_reset(&parser);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    feed_raw(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
