// SPDX-License-Identifier: MIT
// LibFuzzer harness for intel_hex -- coverage-guided fuzzing
//
// Decodes the input as raw text, then as hex lines with a correct checksum whose
// count field can disagree with the data that follows, then through the encoder.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_intel_hex.c ../intel_hex.c -o fuzz_intel_hex
// Run:     ./fuzz_intel_hex -max_total_time=60 corpus_intel_hex/

#include "../intel_hex.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void put_byte(char **p, uint8_t value, uint8_t *sum) {
    static const char digits[] = "0123456789ABCDEF";
    *(*p)++ = digits[value >> 4];
    *(*p)++ = digits[value & 0x0F];
    *sum = (uint8_t)(*sum + value);
}

static void decode_any_count(fuzz_input_t *in) {
    uint8_t count = fuzz_u8(in);
    uint16_t address = fuzz_u16(in);
    uint8_t type = fuzz_u8(in);
    uint8_t body[INTEL_HEX_MAX_DATA + 8];
    size_t wanted = fuzz_u8(in) % (sizeof(body) + 1);
    size_t n = fuzz_take(in, body, wanted);

    // Exact-size line without CRLF, so ASan sees any read past the checksum.
    size_t len = 1 + (4 + n + 1) * 2;
    char *line = malloc(len);
    if (!line) return;

    char *p = line;
    uint8_t sum = 0;
    *p++ = ':';
    put_byte(&p, count, &sum);
    put_byte(&p, (uint8_t)(address >> 8), &sum);
    put_byte(&p, (uint8_t)address, &sum);
    put_byte(&p, type, &sum);
    for (size_t i = 0; i < n; i++) put_byte(&p, body[i], &sum);
    put_byte(&p, (uint8_t)(0u - sum), &sum);

    intel_hex_record_t rec;
    intel_hex_err_t err = intel_hex_decode(line, len, &rec);
    if (count == n && n <= INTEL_HEX_MAX_DATA) {
        if (err != INTEL_HEX_OK || rec.data_len != n || rec.address != address ||
            memcmp(rec.data, body, n) != 0) {
            __builtin_trap();
        }
    }
    free(line);
}

static void round_trip(fuzz_input_t *in) {
    intel_hex_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = (intel_hex_type_t)(fuzz_u8(in) % (INTEL_HEX_START_LINEAR + 1));
    rec.address = fuzz_u16(in);
    rec.data_len = fuzz_u8(in);
    fuzz_take(in, rec.data, sizeof(rec.data));

    // Exact-size output, so ASan sees any write past the CRLF.
    size_t need = 13 + (size_t)rec.data_len * 2;
    char *line = malloc(need);
    if (!line) return;

    size_t len = 0;
    if (intel_hex_encode(&rec, line, need - 1, &len) != INTEL_HEX_ERR_OVERFLOW) __builtin_trap();

    intel_hex_err_t err = intel_hex_encode(&rec, line, need, &len);
    if (rec.data_len > INTEL_HEX_MAX_DATA) {
        if (err != INTEL_HEX_ERR_OVERFLOW) __builtin_trap();
    } else {
        intel_hex_record_t back;
        if (err != INTEL_HEX_OK || len != need) __builtin_trap();
        if (intel_hex_decode(line, len, &back) != INTEL_HEX_OK || back.type != rec.type ||
            back.address != rec.address || back.data_len != rec.data_len ||
            memcmp(back.data, rec.data, rec.data_len) != 0) {
            __builtin_trap();
        }
    }
    free(line);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    intel_hex_record_t rec;
    intel_hex_decode((const char *)data, size, &rec);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        decode_any_count(&in);
        round_trip(&in);
    }

    return 0;
}
