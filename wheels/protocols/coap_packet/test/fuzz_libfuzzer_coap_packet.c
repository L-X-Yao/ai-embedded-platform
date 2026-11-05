// SPDX-License-Identifier: MIT
// LibFuzzer harness for coap_packet — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_coap_packet.c ../coap_packet.c -o fuzz_coap_packet
// Run:     ./fuzz_coap_packet -max_total_time=60 corpus_coap_packet/

#include "../coap_packet.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define VALUE_MAX 300
#define PAYLOAD_MAX 300

typedef struct {
    coap_packet_type_t   type;
    uint8_t              code;
    uint16_t             id;
    uint8_t              token[8];
    uint8_t              token_len;
    coap_packet_option_t options[COAP_MAX_OPTIONS];
    int                  option_count;
    uint8_t              payload[PAYLOAD_MAX];
    size_t               payload_len;
} message_t;

// Deltas and lengths past 12 and 268 take one and two more header bytes.
static size_t extended_size(uint32_t value) {
    if (value < 13) return 0;
    return value < 269 ? 1 : 2;
}

// Fill m from the input and return the size of its frame.
static size_t fill_message(message_t *m, fuzz_input_t *in) {
    static uint8_t values[COAP_MAX_OPTIONS][VALUE_MAX];
    uint8_t op = fuzz_u8(in);

    memset(m, 0, sizeof(*m));
    m->type = (coap_packet_type_t)(op & 0x03);
    m->code = fuzz_u8(in);
    m->id = fuzz_u16(in);
    m->token_len = (uint8_t)(fuzz_u8(in) % 9);
    fuzz_take(in, m->token, m->token_len);
    m->option_count = fuzz_u8(in) % (COAP_MAX_OPTIONS + 1);

    size_t need = 4 + m->token_len;
    uint32_t number = 0;
    for (int i = 0; i < m->option_count; i++) {
        uint32_t delta = fuzz_u16(in);
        if (op & 0x04) delta %= 300;
        if (number + delta > UINT16_MAX) delta = 0;
        number += delta;

        size_t len = fuzz_u16(in) % (VALUE_MAX + 1);
        memset(values[i], 0x5A, len);
        fuzz_take(in, values[i], len);

        m->options[i].number = (uint16_t)number;
        m->options[i].value = len > 0 ? values[i] : NULL;
        m->options[i].value_len = len;
        need += 1 + extended_size(delta) + extended_size((uint32_t)len) + len;
    }

    m->payload_len = fuzz_u16(in) % (PAYLOAD_MAX + 1);
    memset(m->payload, 0xA5, m->payload_len);
    fuzz_take(in, m->payload, m->payload_len);
    if (m->payload_len > 0) need += 1 + m->payload_len;
    return need;
}

static coap_packet_err_t build(uint8_t *wire, size_t cap, size_t *out_len, const message_t *m) {
    return coap_packet_build(wire, cap, out_len, m->type, m->code, m->id,
                             m->token_len > 0 ? m->token : NULL, m->token_len,
                             m->option_count > 0 ? m->options : NULL, m->option_count,
                             m->payload_len > 0 ? m->payload : NULL, m->payload_len);
}

static void check_packet(const message_t *m, const uint8_t *wire, size_t len) {
    coap_packet_t pkt;
    if (coap_packet_parse(wire, len, &pkt) != COAP_PACKET_OK || pkt.type != m->type ||
        pkt.code != m->code || pkt.message_id != m->id || pkt.token_len != m->token_len ||
        memcmp(pkt.token, m->token, m->token_len) != 0 ||
        pkt.option_count != m->option_count || pkt.payload_len != m->payload_len ||
        pkt.total_len != len) {
        __builtin_trap();
    }
    for (int i = 0; i < m->option_count; i++) {
        const coap_packet_option_t *want = &m->options[i];
        const coap_packet_option_t *got = &pkt.options[i];
        if (got->number != want->number || got->value_len != want->value_len ||
            (want->value_len > 0 && memcmp(got->value, want->value, want->value_len) != 0)) {
            __builtin_trap();
        }
    }
    if (m->payload_len > 0 && memcmp(pkt.payload, m->payload, m->payload_len) != 0) {
        __builtin_trap();
    }
}

// A length with no bytes would announce content the frame does not hold.
static void check_missing_bytes(message_t *m, uint8_t *wire, size_t need) {
    size_t out_len = 0;
    if (m->token_len > 0 &&
        coap_packet_build(wire, need, &out_len, m->type, m->code, m->id, NULL, m->token_len,
                          m->options, m->option_count, NULL, 0) == COAP_PACKET_OK) {
        __builtin_trap();
    }
    for (int i = 0; i < m->option_count; i++) {
        const uint8_t *value = m->options[i].value;
        if (m->options[i].value_len == 0) continue;
        m->options[i].value = NULL;
        if (build(wire, need, &out_len, m) == COAP_PACKET_OK) __builtin_trap();
        m->options[i].value = value;
    }
    if (m->payload_len > 0 &&
        coap_packet_build(wire, need, &out_len, m->type, m->code, m->id, m->token, m->token_len,
                          m->options, m->option_count, NULL, m->payload_len) == COAP_PACKET_OK) {
        __builtin_trap();
    }
}

static void round_trip(fuzz_input_t *in) {
    static message_t m;
    size_t need = fill_message(&m, in);
    uint8_t *wire = malloc(need);
    if (!wire) return;

    size_t out_len = 0;
    if (build(wire, need - 1, &out_len, &m) == COAP_PACKET_OK) __builtin_trap();
    if (build(wire, need, &out_len, &m) != COAP_PACKET_OK || out_len != need) __builtin_trap();
    check_packet(&m, wire, need);
    check_missing_bytes(&m, wire, need);
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    coap_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    coap_packet_parse(data, size, &pkt);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip(&in);
    }

    return 0;
}
