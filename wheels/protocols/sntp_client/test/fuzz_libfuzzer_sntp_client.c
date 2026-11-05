// SPDX-License-Identifier: MIT
// LibFuzzer harness for sntp_client -- coverage-guided fuzzing
//
// Parses the input as raw bytes, then forces a valid mode and stratum so the
// timestamp fields reach the clock math.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          -DCONTRACT_ENABLED fuzz_libfuzzer_sntp_client.c ../sntp_client.c -o fuzz_sntp_client
// Run:     ./fuzz_sntp_client -max_total_time=60 corpus_sntp_client/

#include "../sntp_client.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static sntp_timestamp_t read_timestamp(fuzz_input_t *in) {
    sntp_timestamp_t ts;
    uint32_t hi = fuzz_u16(in);
    ts.seconds = (hi << 16) | fuzz_u16(in);
    hi = fuzz_u16(in);
    ts.fraction = (hi << 16) | fuzz_u16(in);
    return ts;
}

static void check_conversions(const sntp_timestamp_t *ts) {
    uint32_t unix_sec = 0;
    uint16_t ms = 0;
    sntp_client_ntp_to_unix(ts, &unix_sec, &ms);
    if (ms >= 1000) __builtin_trap();
    if (ts->seconds < (uint32_t)SNTP_UNIX_OFFSET && unix_sec != 0) __builtin_trap();

    sntp_timestamp_t back = sntp_client_unix_to_ntp(unix_sec, ms);
    uint32_t back_sec = 0;
    uint16_t back_ms = 0;
    sntp_client_ntp_to_unix(&back, &back_sec, &back_ms);
    if (back_sec != unix_sec) __builtin_trap();

    if (back_ms != ms) __builtin_trap();
}

static void check_packet(const uint8_t *data, fuzz_input_t *in) {
    // Exact-size packet, so ASan sees any read past the last timestamp.
    uint8_t *packet = malloc(SNTP_PACKET_SIZE);
    if (!packet) return;
    memcpy(packet, data, SNTP_PACKET_SIZE);
    packet[0] = (uint8_t)((packet[0] & 0xF8) | 4);
    if (packet[1] == 0) packet[1] = 1;

    sntp_response_t resp;
    if (sntp_client_parse_response(packet, SNTP_PACKET_SIZE, &resp) != SNTP_CLIENT_OK) {
        __builtin_trap();
    }
    free(packet);

    sntp_timestamp_t t1 = read_timestamp(in);
    sntp_timestamp_t t4 = read_timestamp(in);
    sntp_result_t result;
    if (sntp_client_compute(&resp, &t1, &t4, &result) != SNTP_CLIENT_OK) __builtin_trap();
    if (result.unix_ms >= 1000) __builtin_trap();

    check_conversions(&resp.transmit);
    check_conversions(&t1);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    sntp_response_t resp;
    sntp_client_parse_response(data, size, &resp);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    uint8_t *request = malloc(SNTP_PACKET_SIZE);
    if (request) {
        sntp_timestamp_t tx = read_timestamp(&in);
        if (sntp_client_build_request(request, SNTP_PACKET_SIZE, &tx) != SNTP_PACKET_SIZE) {
            __builtin_trap();
        }
        free(request);
    }

    if (size >= SNTP_PACKET_SIZE) check_packet(data, &in);

    return 0;
}
