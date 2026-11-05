// SPDX-License-Identifier: MIT
// LibFuzzer harness for modbus_ascii -- coverage-guided fuzzing
//
// Decodes the input as raw text, then between the frame delimiters to reach the
// hex decoder, then through the encoder so frames get past the LRC.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_modbus_ascii.c ../modbus_ascii.c -o fuzz_modbus_ascii
// Run:     ./fuzz_modbus_ascii -max_total_time=60 corpus_modbus_ascii/

#include "../modbus_ascii.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void decode_delimited(const uint8_t *data, size_t size) {
    // Exact-size line, so ASan sees any read past the CRLF.
    char *line = malloc(size + 3);
    if (!line) return;

    line[0] = ':';
    if (size > 0) memcpy(&line[1], data, size);
    line[size + 1] = '\r';
    line[size + 2] = '\n';

    modbus_ascii_frame_t frame;
    modbus_ascii_decode(line, size + 3, &frame);
    free(line);
}

static void lower_hex(char *line, size_t len) {
    for (size_t i = 1; i + 2 < len; i++) {
        if (line[i] >= 'A' && line[i] <= 'F') line[i] = (char)(line[i] + 32);
    }
}

// Record is [op][addr][func][data_len][data]:
//   op bit 0: lower-case hex, which the decoder accepts as well
static void round_trip(fuzz_input_t *in) {
    uint8_t op = fuzz_u8(in);
    modbus_ascii_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.addr = fuzz_u8(in);
    frame.func = fuzz_u8(in);
    frame.data_len = fuzz_u8(in);
    fuzz_take(in, frame.data, sizeof(frame.data));

    size_t need = 9 + (size_t)frame.data_len * 2;
    char *line = malloc(need);
    if (!line) return;

    size_t len = 0;
    modbus_ascii_err_t err = modbus_ascii_encode(&frame, line, need - 1, &len);
    if (err != MODBUS_ASCII_ERR_OVERFLOW) __builtin_trap();

    err = modbus_ascii_encode(&frame, line, need, &len);
    if (frame.data_len > MODBUS_ASCII_MAX_DATA) {
        if (err != MODBUS_ASCII_ERR_OVERFLOW) __builtin_trap();
    } else {
        if (err != MODBUS_ASCII_OK || len != need) __builtin_trap();
        if (op & 1) lower_hex(line, len);

        modbus_ascii_frame_t back;
        if (modbus_ascii_decode(line, len, &back) != MODBUS_ASCII_OK ||
            back.addr != frame.addr || back.func != frame.func ||
            back.data_len != frame.data_len ||
            memcmp(back.data, frame.data, frame.data_len) != 0) {
            __builtin_trap();
        }
    }
    free(line);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    modbus_ascii_frame_t frame;
    modbus_ascii_decode((const char *)data, size, &frame);
    decode_delimited(data, size);
    (void)modbus_ascii_lrc(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
