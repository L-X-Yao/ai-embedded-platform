// SPDX-License-Identifier: MIT
// LibFuzzer harness for at_command -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_at_command.c ../at_command.c -o fuzz_at_command
// Run:     ./fuzz_at_command -max_total_time=60 corpus_at_command/

#include "../at_command.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const char *const expectations[] = { "+CSQ", "OK", ">", "CONNECT" };
#define EXPECTATION_COUNT (sizeof(expectations) / sizeof(expectations[0]))

static uint32_t g_urc_calls;

static void dummy_send(void *ctx, const uint8_t *data, uint16_t len) {
    (void)ctx; (void)data; (void)len;
}

static void count_urc(const char *line, void *ctx) {
    (void)ctx;
    if (line[0] == '\0') __builtin_trap();
    g_urc_calls++;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    at_command_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.send = dummy_send;
    hal.ctx = NULL;

    at_command_t at;
    memset(&at, 0, sizeof(at));
    at_command_init(&at, &hal);

    // A NULL callback is rejected, so URC dispatch needs a real one.
    at_command_register_urc(&at, "+CREG", count_urc, NULL);
    at_command_register_urc(&at, "+CGREG", count_urc, NULL);

    at_command_send(&at, "AT+CSQ");
    if (size > 0) {
        at_command_send_raw(&at, data, (uint16_t)(size > 0xFFFF ? 0xFFFF : size));
    }

    // The first byte picks the expectation; start with none to reach URC lines.
    if (size > 0 && data[0] < 0xF0) {
        at_command_expect(&at, expectations[data[0] % EXPECTATION_COUNT]);
    }

    // Feed all bytes one at a time through the state machine
    for (size_t i = 0; i < size; i++) {
        at_command_evt_t evt;
        memset(&evt, 0, sizeof(evt));
        at_command_evt_type_t type = at_command_feed(&at, data[i], &evt);

        if (evt.line && strlen(evt.line) != evt.line_len) __builtin_trap();

        // A finished exchange arms the next one, as a modem driver would.
        if (type == AT_COMMAND_EVT_OK || type == AT_COMMAND_EVT_ERROR ||
            type == AT_COMMAND_EVT_PROMPT) {
            const char *next = expectations[data[i] % EXPECTATION_COUNT];
            if (data[i] & 0x10U) {
                at_command_expect_final(&at, next);
            } else {
                at_command_expect(&at, next);
            }
        }
    }

    // Exercise accessors
    (void)at_command_get_matched(&at);
    (void)at_command_get_prev_line(&at);

    // Cleanup
    at_command_cancel(&at);
    at_command_unregister_urc(&at, "+CREG");
    at_command_unregister_urc(&at, "+CGREG");
    at_command_unregister_urc(&at, "+CGREG");

    return 0;
}
