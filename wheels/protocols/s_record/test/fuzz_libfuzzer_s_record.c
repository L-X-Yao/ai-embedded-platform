// SPDX-License-Identifier: MIT
// LibFuzzer harness for s_record -- coverage-guided fuzzing
//
// Decodes the input as raw text, then as hex lines with a correct checksum whose
// count field can disagree with the bytes that follow, then through the encoder.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_s_record.c ../s_record.c -o fuzz_s_record
// Run:     ./fuzz_s_record -max_total_time=60 corpus_s_record/

#include "../s_record.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const s_record_type_t types[] = {
    S_RECORD_S0, S_RECORD_S1, S_RECORD_S2, S_RECORD_S3,
    S_RECORD_S5, S_RECORD_S7, S_RECORD_S8, S_RECORD_S9,
};
#define TYPE_COUNT (sizeof(types) / sizeof(types[0]))

static int address_bytes(s_record_type_t type) {
    switch (type) {
    case S_RECORD_S2: case S_RECORD_S8: return 3;
    case S_RECORD_S3: case S_RECORD_S7: return 4;
    default: return 2;
    }
}

static void put_byte(char **p, uint8_t value, uint8_t *sum) {
    static const char digits[] = "0123456789ABCDEF";
    *(*p)++ = digits[value >> 4];
    *(*p)++ = digits[value & 0x0F];
    *sum = (uint8_t)(*sum + value);
}

static void decode_any_count(fuzz_input_t *in) {
    s_record_type_t type = types[fuzz_u8(in) % TYPE_COUNT];
    uint8_t count = fuzz_u8(in);
    uint8_t body[64];
    size_t wanted = fuzz_u8(in) % (sizeof(body) + 1);
    size_t n = fuzz_take(in, body, wanted);

    // Exact-size line without CRLF, so ASan sees any read past the checksum.
    size_t len = 4 + (n + 1) * 2;
    char *line = malloc(len);
    if (!line) return;

    char *p = line;
    uint8_t sum = 0;
    *p++ = 'S';
    *p++ = (char)('0' + (int)type);
    put_byte(&p, count, &sum);
    for (size_t i = 0; i < n; i++) put_byte(&p, body[i], &sum);
    put_byte(&p, (uint8_t)~sum, &sum);

    // A record has to hold its address and checksum, however the count reads.
    s_record_line_t rec;
    if (s_record_decode(line, len, &rec) == S_RECORD_OK &&
        count < address_bytes(type) + 1) {
        __builtin_trap();
    }
    free(line);
}

static void round_trip(fuzz_input_t *in) {
    s_record_line_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = types[fuzz_u8(in) % TYPE_COUNT];
    rec.data_len = fuzz_u8(in);
    uint32_t hi = fuzz_u16(in);
    rec.address = (hi << 16) | fuzz_u16(in);
    fuzz_take(in, rec.data, sizeof(rec.data));

    int ab = address_bytes(rec.type);
    uint32_t mask = ab == 4 ? 0xFFFFFFFFu : ((1u << (ab * 8)) - 1u);
    if (fuzz_u8(in) & 1) rec.address &= mask;

    // Exact-size output, so ASan sees any write past the CRLF.
    size_t need = 4 + ((size_t)ab + rec.data_len + 1) * 2 + 2;
    char *line = malloc(need);
    if (!line) return;

    size_t len = 0;
    if (s_record_encode(&rec, line, need - 1, &len) != S_RECORD_ERR_OVERFLOW) __builtin_trap();

    s_record_err_t err = s_record_encode(&rec, line, need, &len);
    if (rec.data_len > S_RECORD_MAX_DATA || (rec.address & ~mask) != 0) {
        if (err != S_RECORD_ERR_OVERFLOW) __builtin_trap();
    } else {
        s_record_line_t back;
        if (err != S_RECORD_OK || len != need) __builtin_trap();
        if (s_record_decode(line, len, &back) != S_RECORD_OK || back.type != rec.type ||
            back.address != rec.address || back.data_len != rec.data_len ||
            memcmp(back.data, rec.data, rec.data_len) != 0) {
            __builtin_trap();
        }
    }
    free(line);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    s_record_line_t rec;
    s_record_decode((const char *)data, size, &rec);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        decode_any_count(&in);
        round_trip(&in);
    }

    return 0;
}
