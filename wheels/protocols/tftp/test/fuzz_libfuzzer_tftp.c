// SPDX-License-Identifier: MIT
// LibFuzzer harness for tftp — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_tftp.c ../tftp.c -o fuzz_tftp
// Run:     ./fuzz_tftp -max_total_time=60 corpus_tftp/

#include "../tftp.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define NAME_CAP (TFTP_MAX_FILENAME + 40)
#define MODE_CAP 24
#define MESSAGE_CAP 160

static void round_trip_request(fuzz_input_t *in) {
    char filename[NAME_CAP + 1];
    char mode[MODE_CAP + 1];
    bool write = (fuzz_u8(in) & 1) != 0;
    size_t want = fuzz_u8(in) % (NAME_CAP + 1);
    size_t name_len = fuzz_take_str(in, filename, want);
    want = fuzz_u8(in) % (MODE_CAP + 1);
    size_t mode_len = fuzz_take_str(in, mode, want);

    size_t need = 2 + name_len + 1 + mode_len + 1;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    int (*build)(uint8_t *, size_t, const char *, const char *) =
        write ? tftp_build_wrq : tftp_build_rrq;

    if (build(wire, need - 1, filename, mode) >= 0) __builtin_trap();

    int len = build(wire, need, filename, mode);
    if (name_len == 0 || name_len >= TFTP_MAX_FILENAME) {
        if (len >= 0) __builtin_trap();
    } else {
        tftp_packet_t pkt;
        if (len != (int)need || tftp_parse(wire, need, &pkt) != TFTP_OK ||
            pkt.opcode != (write ? TFTP_OP_WRQ : TFTP_OP_RRQ) ||
            strcmp(pkt.u.rq.filename, filename) != 0 ||
            strncmp(pkt.u.rq.mode, mode, sizeof(pkt.u.rq.mode) - 1) != 0) {
            __builtin_trap();
        }
    }
    free(wire);
}

static void round_trip_data(fuzz_input_t *in) {
    uint8_t bytes[TFTP_BLOCK_SIZE + 8];
    uint16_t block = fuzz_u16(in);
    size_t len = fuzz_u16(in) % sizeof(bytes);
    memset(bytes, 0x5A, len);
    fuzz_take(in, bytes, len);

    size_t need = 4 + len;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    const uint8_t *src = len > 0 ? bytes : NULL;
    if (tftp_build_data(wire, need - 1, block, src, len) >= 0) __builtin_trap();

    int n = tftp_build_data(wire, need, block, src, len);
    if (len > TFTP_BLOCK_SIZE) {
        if (n >= 0) __builtin_trap();
    } else {
        tftp_packet_t pkt;
        if (n != (int)need || tftp_parse(wire, need, &pkt) != TFTP_OK ||
            pkt.opcode != TFTP_OP_DATA || pkt.u.data.block != block ||
            pkt.u.data.len != len || (len > 0 && memcmp(pkt.u.data.data, bytes, len) != 0) ||
            tftp_is_last_data(&pkt) != (len < TFTP_BLOCK_SIZE)) {
            __builtin_trap();
        }
    }
    free(wire);
}

static void round_trip_ack(uint16_t block) {
    tftp_packet_t pkt;
    uint8_t *wire = malloc(4);
    if (!wire) return;

    if (tftp_build_ack(wire, 3, block) >= 0) __builtin_trap();
    if (tftp_build_ack(wire, 4, block) != 4 || tftp_parse(wire, 4, &pkt) != TFTP_OK ||
        pkt.opcode != TFTP_OP_ACK || pkt.u.ack.block != block) {
        __builtin_trap();
    }
    free(wire);
}

static void round_trip_error(fuzz_input_t *in) {
    char message[MESSAGE_CAP + 1];
    uint16_t code = fuzz_u16(in);
    size_t want = fuzz_u8(in) % (MESSAGE_CAP + 1);
    size_t len = fuzz_take_str(in, message, want);

    size_t need = 4 + len + 1;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (tftp_build_error(wire, need - 1, code, message) >= 0) __builtin_trap();

    // The parser keeps as much of the message as its field holds.
    tftp_packet_t pkt;
    size_t kept = len < sizeof(pkt.u.error.msg) ? len : sizeof(pkt.u.error.msg) - 1;
    if (tftp_build_error(wire, need, code, message) != (int)need ||
        tftp_parse(wire, need, &pkt) != TFTP_OK || pkt.opcode != TFTP_OP_ERROR ||
        pkt.u.error.code != code || strlen(pkt.u.error.msg) != kept ||
        memcmp(pkt.u.error.msg, message, kept) != 0) {
        __builtin_trap();
    }
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    int rc = tftp_parse(data, size, &pkt);

    /* Exercise is_last_data on any successfully parsed DATA packet */
    if (rc == TFTP_OK && pkt.opcode == TFTP_OP_DATA) {
        tftp_is_last_data(&pkt);
    }

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip_request(&in);
        round_trip_data(&in);
        round_trip_ack(fuzz_u16(&in));
        round_trip_error(&in);
    }

    return 0;
}
