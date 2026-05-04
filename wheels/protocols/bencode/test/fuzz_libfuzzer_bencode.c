// SPDX-License-Identifier: MIT
// LibFuzzer harness for bencode — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_bencode.c ../bencode.c -o fuzz_bencode
// Run:     ./fuzz_bencode -max_total_time=60 corpus_bencode/

#include "../bencode.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    bencode_decoder_t dec;
    bencode_decoder_init(&dec, data, size);

    // Drain all tokens until error or exhaustion
    bencode_token_t token;
    for (int i = 0; i < 4096; i++) {
        bencode_err_t err = bencode_decode_next(&dec, &token);
        if (err != BENCODE_OK) break;
    }

    return 0;
}
