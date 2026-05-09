// SPDX-License-Identifier: MIT
// LibFuzzer harness for tftp — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_tftp.c ../tftp.c -o fuzz_tftp
// Run:     ./fuzz_tftp -max_total_time=60 corpus_tftp/

#include "../tftp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    int rc = tftp_parse(data, size, &pkt);

    /* Exercise is_last_data on any successfully parsed DATA packet */
    if (rc == TFTP_OK && pkt.opcode == TFTP_OP_DATA) {
        tftp_is_last_data(&pkt);
    }

    return 0;
}
