// SPDX-License-Identifier: MIT
// LibFuzzer harness for msgpack — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_msgpack.c ../msgpack.c -o fuzz_msgpack
// Run:     ./fuzz_msgpack -max_total_time=60 corpus_msgpack/

#include "../msgpack.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    msgpack_dec_t dec;
    msgpack_dec_init(&dec, data, size);

    // Drain all tokens until error or exhaustion
    msgpack_token_t tok;
    for (int i = 0; i < 4096; i++) {
        msgpack_err_t err = msgpack_dec_next(&dec, &tok);
        if (err != MSGPACK_OK) break;
    }

    return 0;
}
