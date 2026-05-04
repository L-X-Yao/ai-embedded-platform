// SPDX-License-Identifier: MIT
// LibFuzzer harness for tlv_codec — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_tlv_codec.c ../tlv_codec.c -o fuzz_tlv_codec
// Run:     ./fuzz_tlv_codec -max_total_time=60 corpus_tlv_codec/

#include "../tlv_codec.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, data, len);

    // Drain all TLV elements until error or exhaustion
    tlv_t tlv;
    for (int i = 0; i < 4096; i++) {
        tlv_err_t err = tlv_decode_next(&dec, &tlv);
        if (err != TLV_OK) break;

        // Exercise value extraction on each decoded element
        tlv_get_u8(&tlv);
        tlv_get_u16(&tlv);
        tlv_get_u32(&tlv);
    }

    return 0;
}
