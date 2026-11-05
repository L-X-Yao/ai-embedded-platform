// SPDX-License-Identifier: MIT
// LibFuzzer harness for uds -- coverage-guided fuzzing
//
// Decodes the input, then encodes the decoded message again: a request or positive
// response has to reproduce the input exactly.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_uds.c ../uds.c -o fuzz_uds
// Run:     ./fuzz_uds -max_total_time=60 corpus_uds/

#include "../uds.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void check_negative(const uint8_t *data, const uds_msg_t *msg) {
    uint8_t *out = malloc(3);
    if (!out) return;

    size_t len = 0;
    if (uds_encode_negative(msg->rejected_sid, msg->nrc, out, 2, &len) != UDS_ERR_OVERFLOW) {
        __builtin_trap();
    }
    if (uds_encode_negative(msg->rejected_sid, msg->nrc, out, 3, &len) != UDS_OK ||
        len != 3 || memcmp(out, data, 3) != 0) {
        __builtin_trap();
    }
    free(out);
}

static void check_plain(const uint8_t *data, size_t size, const uds_msg_t *msg) {
    // Exact-size output, so ASan sees any write past the end.
    uint8_t *out = malloc(size);
    if (!out) return;

    size_t len = 0;
    uds_err_t err;
    if (msg->is_response) {
        err = uds_encode_response(msg->sid, msg->data, msg->data_len, out, size - 1, &len);
    } else {
        err = uds_encode_request(msg->sid, msg->data, msg->data_len, out, size - 1, &len);
    }
    if (err != UDS_ERR_OVERFLOW) __builtin_trap();

    if (msg->is_response) {
        err = uds_encode_response(msg->sid, msg->data, msg->data_len, out, size, &len);
    } else {
        err = uds_encode_request(msg->sid, msg->data, msg->data_len, out, size, &len);
    }
    if (err != UDS_OK || len != size || memcmp(out, data, size) != 0) __builtin_trap();
    free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uds_msg_t msg;
    if (uds_decode(data, size, &msg) != UDS_OK) return 0;

    if (msg.data_len > UDS_MAX_DATA) __builtin_trap();

    if (msg.is_negative) {
        check_negative(data, &msg);
    } else {
        check_plain(data, size, &msg);
    }

    return 0;
}
