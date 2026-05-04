// SPDX-License-Identifier: MIT
// LibFuzzer harness for cbor_lite — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_cbor_lite.c ../cbor_lite.c -o fuzz_cbor_lite
// Run:     ./fuzz_cbor_lite -max_total_time=60 corpus_cbor_lite/

#include "../cbor_lite.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, data, size);

    // Drain all items until error or exhaustion
    cbor_lite_item_t item;
    for (int i = 0; i < 4096; i++) {
        cbor_lite_err_t err = cbor_lite_dec_next(&dec, &item);
        if (err != CBOR_LITE_OK) break;
    }

    return 0;
}
