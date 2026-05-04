// SPDX-License-Identifier: MIT
// LibFuzzer harness for protobuf_lite — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_protobuf_lite.c ../protobuf_lite.c -o fuzz_protobuf_lite
// Run:     ./fuzz_protobuf_lite -max_total_time=60 corpus_protobuf_lite/

#include "../protobuf_lite.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    pb_decoder_t dec;
    pb_decoder_init(&dec, data, len);

    // Drain all fields until error or exhaustion
    pb_field_t field;
    for (int i = 0; i < 4096; i++) {
        pb_err_t err = pb_decode_field(&dec, &field);
        if (err != PB_OK) break;
    }

    return 0;
}
