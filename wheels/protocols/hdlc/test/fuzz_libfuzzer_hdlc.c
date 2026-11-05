// SPDX-License-Identifier: MIT
// LibFuzzer harness for hdlc -- coverage-guided fuzzing
//
// Feeds the input to the parser as raw bytes, then encodes it so frames get past
// the FCS. Decoded payloads have to match what was encoded.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_hdlc.c ../hdlc.c -o fuzz_hdlc
// Run:     ./fuzz_hdlc -max_total_time=60 corpus_hdlc/

#include "../hdlc.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PAYLOAD_MAX 512

static void extract(const hdlc_parser_t *parser, size_t cap) {
    // Exact-size output, so ASan sees any write past cap.
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    size_t out_len = 0;
    if (hdlc_parser_get_frame(parser, out, cap, &out_len) == HDLC_OK && out_len > cap) {
        __builtin_trap();
    }
    free(out);
}

static void feed_raw(const uint8_t *data, size_t size) {
    if (size == 0) return;

    // Exact-size parse buffer; the first byte picks its capacity.
    size_t capacity = data[0] ? data[0] : 1;
    uint8_t *buf = malloc(capacity);
    if (!buf) return;

    hdlc_parser_t parser;
    if (hdlc_parser_init(&parser, buf, capacity) != HDLC_OK) __builtin_trap();

    for (size_t i = 1; i < size; i++) {
        if (hdlc_parser_feed(&parser, data[i]) == HDLC_STATE_COMPLETE) {
            extract(&parser, data[i] % (capacity + 1));
            if (data[i] & 1) hdlc_parser_reset(&parser);
        }
    }
    free(buf);
}

// Encoding into exactly the reported size works, one byte less overflows.
static void check_tight(const uint8_t *payload, size_t n, size_t wire_len) {
    uint8_t *tight = malloc(wire_len);
    if (!tight) return;

    size_t len = 0;
    if (hdlc_encode(payload, n, tight, wire_len, &len) != HDLC_OK || len != wire_len) {
        __builtin_trap();
    }
    if (hdlc_encode(payload, n, tight, wire_len - 1, &len) != HDLC_ERR_OVERFLOW) {
        __builtin_trap();
    }
    free(tight);
}

static void parse_back(const uint8_t *wire, size_t wire_len, const uint8_t *payload,
                       size_t n, uint8_t pick) {
    // Exact-size buffers: payload plus FCS in the parser, payload on the way out.
    uint8_t *buf = malloc(n + 2);
    uint8_t *out = malloc(n ? n : 1);
    if (buf && out) {
        hdlc_parser_t parser;
        if (hdlc_parser_init(&parser, buf, n + 2) != HDLC_OK) __builtin_trap();

        hdlc_state_t state = HDLC_STATE_IDLE;
        for (size_t i = 0; i < wire_len; i++) state = hdlc_parser_feed(&parser, wire[i]);
        if (state != HDLC_STATE_COMPLETE) __builtin_trap();

        size_t out_len = 0;
        if (hdlc_parser_get_frame(&parser, out, n, &out_len) != HDLC_OK || out_len != n ||
            (n > 0 && memcmp(out, payload, n) != 0)) {
            __builtin_trap();
        }

        // One flipped bit has to fail the FCS.
        if (n > 0) {
            buf[pick % n] ^= (uint8_t)(1u << (pick >> 5));
            if (hdlc_parser_get_frame(&parser, out, n, &out_len) != HDLC_ERR_FCS) {
                __builtin_trap();
            }
        }
    }
    free(buf);
    free(out);
}

static void round_trip(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    uint8_t pick = fuzz_u8(&in);
    uint8_t payload[PAYLOAD_MAX];
    size_t n = fuzz_take(&in, payload, sizeof(payload));

    // Worst case escapes every byte of payload and FCS, plus two flags.
    size_t need = (n + 2) * 2 + 2;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    size_t wire_len = 0;
    if (hdlc_encode(payload, n, wire, need, &wire_len) != HDLC_OK) __builtin_trap();

    check_tight(payload, n, wire_len);
    parse_back(wire, wire_len, payload, n, pick);
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    feed_raw(data, size);
    round_trip(data, size);
    return 0;
}
