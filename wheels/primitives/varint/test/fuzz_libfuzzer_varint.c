// SPDX-License-Identifier: MIT
// LibFuzzer harness for varint -- coverage-guided fuzzing
//
// Decodes the input as a run of varints, then encodes 64-bit values read from the
// input into exact-size buffers and decodes them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_varint.c ../varint.c -o fuzz_varint
// Run:     ./fuzz_varint -max_total_time=60 corpus_varint/

#include "../varint.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void decode_run(const uint8_t *data, size_t size) {
    size_t pos = 0;
    while (pos < size) {
        uint64_t value = 0;
        int64_t signed_value = 0;
        size_t len = 0;
        size_t signed_len = 0;

        if (varint_decode_u64(&data[pos], size - pos, &value, &len) != VARINT_OK) break;
        if (len == 0 || len > size - pos) __builtin_trap();

        if (varint_decode_s64(&data[pos], size - pos, &signed_value, &signed_len) != VARINT_OK ||
            signed_len != len) {
            __builtin_trap();
        }
        pos += len;
    }
}

static uint64_t read_u64(fuzz_input_t *in) {
    uint64_t value = 0;
    uint8_t shift = fuzz_u8(in) % 64;
    for (int i = 0; i < 4; i++) value = (value << 16) | fuzz_u16(in);
    return value >> shift;
}

static void check_unsigned(uint64_t value) {
    size_t need = varint_encoded_size_u64(value);
    if (need == 0 || need > 10) __builtin_trap();

    // Exact-size buffer, so ASan sees any write past cap.
    uint8_t *buf = malloc(need);
    if (!buf) return;

    size_t len = 0;
    if (varint_encode_u64(value, buf, need - 1, &len) != VARINT_ERR_OVERFLOW) __builtin_trap();
    if (varint_encode_u64(value, buf, need, &len) != VARINT_OK || len != need) __builtin_trap();

    uint64_t back = 0;
    size_t used = 0;
    if (varint_decode_u64(buf, len, &back, &used) != VARINT_OK || back != value || used != len) {
        __builtin_trap();
    }
    if (varint_decode_u64(buf, len - 1, &back, &used) != VARINT_ERR_SHORT) __builtin_trap();
    free(buf);
}

static void check_signed(int64_t value) {
    uint8_t buf[10];
    size_t len = 0;
    if (varint_encode_s64(value, buf, sizeof(buf), &len) != VARINT_OK) __builtin_trap();

    int64_t back = 0;
    size_t used = 0;
    if (varint_decode_s64(buf, len, &back, &used) != VARINT_OK || back != value || used != len) {
        __builtin_trap();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    decode_run(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        uint64_t value = read_u64(&in);
        check_unsigned(value);
        check_signed((int64_t)value);
    }

    return 0;
}
