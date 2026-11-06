// SPDX-License-Identifier: MIT
// LibFuzzer harness for ppp_frame -- coverage-guided fuzzing
//
// Decodes the input as raw bytes, then between two flags to reach the destuffing
// loop, then through the encoder so frames get past the FCS.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_ppp_frame.c ../ppp_frame.c -o fuzz_ppp_frame
// Run:     ./fuzz_ppp_frame -max_total_time=60 corpus_ppp_frame/

#include "../ppp_frame.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// The default MRU in RFC 1661. The decoder itself has no limit.
#define PAYLOAD_MAX 1500

static void decode_into(const uint8_t *src, size_t len, size_t cap) {
    // Exact-size output, so ASan sees any write past cap.
    uint8_t *out = malloc(cap ? cap : 1);
    if (!out) return;

    uint16_t protocol = 0;
    size_t out_len = 0;
    if (ppp_frame_decode(src, len, &protocol, out, cap, &out_len) == PPP_FRAME_OK &&
        out_len > cap) {
        __builtin_trap();
    }
    free(out);
}

static void decode_flagged(const uint8_t *data, size_t size, size_t cap) {
    uint8_t *wire = malloc(size + 2);
    if (!wire) return;

    wire[0] = PPP_FLAG;
    if (size > 0) memcpy(&wire[1], data, size);
    wire[size + 1] = PPP_FLAG;
    decode_into(wire, size + 2, cap);
    free(wire);
}

static void check_stuffing(const uint8_t *wire, size_t wire_len) {
    if (wire[0] != PPP_FLAG || wire[wire_len - 1] != PPP_FLAG) __builtin_trap();
    for (size_t i = 1; i + 1 < wire_len; i++) {
        if (wire[i] == PPP_FLAG || wire[i] < 0x20) __builtin_trap();
    }
}

static void decode_back(const uint8_t *wire, size_t wire_len, uint16_t protocol,
                        const uint8_t *payload, size_t n) {
    uint8_t *out = malloc(n ? n : 1);
    if (!out) return;

    uint16_t got_protocol = 0;
    size_t out_len = 0;
    if (ppp_frame_decode(wire, wire_len, &got_protocol, out, n, &out_len) != PPP_FRAME_OK ||
        got_protocol != protocol || out_len != n ||
        (n > 0 && memcmp(out, payload, n) != 0)) {
        __builtin_trap();
    }

    if (n > 0 && ppp_frame_decode(wire, wire_len, &got_protocol, out, n - 1, &out_len) !=
                     PPP_FRAME_ERR_OVERFLOW) {
        __builtin_trap();
    }
    free(out);
}

static void round_trip(fuzz_input_t *in) {
    uint16_t protocol = fuzz_u16(in);
    uint8_t payload[PAYLOAD_MAX];
    size_t n = fuzz_take(in, payload, sizeof(payload));

    // Worst case escapes every header, payload and FCS byte, plus two flags.
    size_t need = (n + 6) * 2 + 2;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    size_t wire_len = 0;
    if (ppp_frame_encode(protocol, payload, n, wire, need, &wire_len) != PPP_FRAME_OK) {
        __builtin_trap();
    }

    size_t short_len = 0;
    if (ppp_frame_encode(protocol, payload, n, wire, wire_len - 1, &short_len) !=
        PPP_FRAME_ERR_OVERFLOW) {
        __builtin_trap();
    }
    if (ppp_frame_encode(protocol, payload, n, wire, wire_len, &short_len) != PPP_FRAME_OK) {
        __builtin_trap();
    }

    check_stuffing(wire, wire_len);
    decode_back(wire, wire_len, protocol, payload, n);
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t cap = size > 0 ? data[0] : 0;

    decode_into(data, size, cap);
    decode_flagged(data, size, cap);
    (void)ppp_frame_fcs16(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
