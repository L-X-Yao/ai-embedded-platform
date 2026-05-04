// SPDX-License-Identifier: MIT
// LibFuzzer harness for coap_packet — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_coap_packet.c ../coap_packet.c -o fuzz_coap_packet
// Run:     ./fuzz_coap_packet -max_total_time=60 corpus_coap_packet/

#include "../coap_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    coap_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    coap_packet_parse(data, size, &pkt);

    return 0;
}
