// SPDX-License-Identifier: MIT
// LibFuzzer harness for at_command -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_at_command.c ../at_command.c -o fuzz_at_command
// Run:     ./fuzz_at_command -max_total_time=60 corpus_at_command/

#include "../at_command.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static void dummy_send(void *ctx, const uint8_t *data, uint16_t len) {
    (void)ctx; (void)data; (void)len;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    at_command_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.send = dummy_send;
    hal.ctx = NULL;

    at_command_t at;
    memset(&at, 0, sizeof(at));
    at_command_init(&at, &hal);

    // Set an expected response to exercise matching paths
    at_command_expect(&at, "+CSQ");

    // Register a URC to exercise URC dispatch paths
    at_command_register_urc(&at, "+CREG", NULL, NULL);

    // Feed all bytes one at a time through the state machine
    for (size_t i = 0; i < size; i++) {
        at_command_evt_t evt;
        memset(&evt, 0, sizeof(evt));
        at_command_feed(&at, data[i], &evt);
    }

    // Exercise accessors
    (void)at_command_get_matched(&at);
    (void)at_command_get_prev_line(&at);

    // Cleanup
    at_command_cancel(&at);
    at_command_unregister_urc(&at, "+CREG");

    return 0;
}
