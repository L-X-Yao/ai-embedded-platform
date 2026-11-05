// SPDX-License-Identifier: MIT
// LibFuzzer harness for dhcp_packet — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_dhcp_packet.c ../dhcp_packet.c -o fuzz_dhcp_packet
// Run:     ./fuzz_dhcp_packet -max_total_time=60 corpus_dhcp_packet/

#include "../dhcp_packet.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_MAX 300

static void check_header(const uint8_t *wire, int len, uint32_t xid,
                         const uint8_t *mac, uint8_t msg_type) {
    dhcp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    if (dhcp_packet_parse(wire, (size_t)len, &hdr) != DHCP_PACKET_OK || hdr.op != 1 ||
        hdr.htype != 1 || hdr.hlen != 6 || hdr.xid != xid ||
        memcmp(hdr.chaddr, mac, 6) != 0 || hdr.msg_type != msg_type) {
        __builtin_trap();
    }
}

static void check_ip_option(const uint8_t *wire, int len, uint8_t code, const uint8_t *want) {
    dhcp_option_t opt;
    uint8_t got[4];
    if (dhcp_packet_get_option(wire, (size_t)len, code, &opt) != DHCP_PACKET_OK) __builtin_trap();

    dhcp_packet_option_ip(&opt, got);
    uint32_t value = dhcp_packet_option_u32(&opt);
    uint32_t expect = ((uint32_t)want[0] << 24) | ((uint32_t)want[1] << 16) |
                      ((uint32_t)want[2] << 8) | want[3];
    if (memcmp(got, want, 4) != 0 || value != expect) __builtin_trap();
}

// Build into each size the input picks, then the exact size, and read it back.
static void round_trip_discover(fuzz_input_t *in) {
    uint8_t scratch[FRAME_MAX];
    uint8_t mac[6] = {0};
    uint32_t xid = ((uint32_t)fuzz_u16(in) << 16) | fuzz_u16(in);
    fuzz_take(in, mac, sizeof(mac));

    int need = dhcp_packet_build_discover(scratch, sizeof(scratch), xid, mac);
    if (need != DHCP_PACKET_BOOTP_MIN_LEN) __builtin_trap();

    size_t cap = fuzz_u16(in) % (size_t)need;
    uint8_t *small = malloc(cap ? cap : 1);
    uint8_t *wire = malloc((size_t)need);
    if (small && wire) {
        if (dhcp_packet_build_discover(small, cap, xid, mac) >= 0) __builtin_trap();
        if (dhcp_packet_build_discover(wire, (size_t)need - 1, xid, mac) >= 0) __builtin_trap();
        if (dhcp_packet_build_discover(wire, (size_t)need, xid, mac) != need) __builtin_trap();
        if (memcmp(wire, scratch, (size_t)need) != 0) __builtin_trap();
        check_header(wire, need, xid, mac, DHCP_MSG_DISCOVER);
    }
    free(wire);
    free(small);
}

static void round_trip_request(fuzz_input_t *in) {
    uint8_t scratch[FRAME_MAX];
    uint8_t mac[6] = {0};
    uint8_t req_ip[4] = {0};
    uint8_t server_ip[4] = {0};
    uint32_t xid = ((uint32_t)fuzz_u16(in) << 16) | fuzz_u16(in);
    fuzz_take(in, mac, sizeof(mac));
    fuzz_take(in, req_ip, sizeof(req_ip));
    fuzz_take(in, server_ip, sizeof(server_ip));

    int need = dhcp_packet_build_request(scratch, sizeof(scratch), xid, mac, req_ip, server_ip);
    if (need != DHCP_PACKET_BOOTP_MIN_LEN) __builtin_trap();

    size_t cap = fuzz_u16(in) % (size_t)need;
    uint8_t *small = malloc(cap ? cap : 1);
    uint8_t *wire = malloc((size_t)need);
    if (small && wire) {
        if (dhcp_packet_build_request(small, cap, xid, mac, req_ip, server_ip) >= 0) {
            __builtin_trap();
        }
        if (dhcp_packet_build_request(wire, (size_t)need - 1, xid, mac, req_ip, server_ip) >= 0) {
            __builtin_trap();
        }
        if (dhcp_packet_build_request(wire, (size_t)need, xid, mac, req_ip, server_ip) != need) {
            __builtin_trap();
        }
        check_header(wire, need, xid, mac, DHCP_MSG_REQUEST);
        check_ip_option(wire, need, DHCP_OPT_REQ_IP, req_ip);
        check_ip_option(wire, need, DHCP_OPT_SERVER_ID, server_ip);
    }
    free(wire);
    free(small);
}

// The accessors must cope with options shorter than the four bytes they read.
static void read_options(const uint8_t *data, size_t size) {
    size_t offset = DHCP_PACKET_MIN_LEN;
    dhcp_option_t opt;
    if (size < DHCP_PACKET_MIN_LEN) return;

    for (int i = 0; i < 256; i++) {
        uint8_t ip[4];
        if (dhcp_packet_next_option(data, size, &offset, &opt) <= 0) break;
        uint32_t value = dhcp_packet_option_u32(&opt);
        dhcp_packet_option_ip(&opt, ip);
        if (opt.len < 4 && (value != 0 || ip[0] != 0)) __builtin_trap();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Parse header */
    dhcp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dhcp_packet_parse(data, size, &hdr);

    /* Iterate all options */
    if (size >= 240) {
        size_t offset = 240;
        dhcp_option_t opt;
        for (int i = 0; i < 256; i++) {
            int rc = dhcp_packet_next_option(data, size, &offset, &opt);
            if (rc <= 0) break;
        }
    }

    /* Try getting specific options */
    dhcp_option_t opt;
    dhcp_packet_get_option(data, size, DHCP_OPT_MSG_TYPE, &opt);
    dhcp_packet_get_option(data, size, DHCP_OPT_SUBNET_MASK, &opt);
    dhcp_packet_get_option(data, size, DHCP_OPT_ROUTER, &opt);

    read_options(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip_discover(&in);
        round_trip_request(&in);
    }

    return 0;
}
