// SPDX-License-Identifier: MIT
// LibFuzzer harness for websocket_frame — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_websocket_frame.c ../websocket_frame.c -o fuzz_websocket_frame
// Run:     ./fuzz_websocket_frame -max_total_time=60 corpus_websocket_frame/

#include "../websocket_frame.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    websocket_frame_parse(data, size, &frame);

    return 0;
}
