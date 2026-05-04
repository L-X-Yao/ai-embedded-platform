// SPDX-License-Identifier: MIT
// LibFuzzer harness for modbus_rtu — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../primitives/crc16 \
//          fuzz_libfuzzer_modbus_rtu.c ../modbus_rtu.c ../../../primitives/crc16/crc16.c \
//          -o fuzz_modbus_rtu
// Run:     ./fuzz_modbus_rtu -max_total_time=60 corpus_modbus_rtu/

#include "../modbus_rtu.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    // Parse frame (validates CRC internally)
    modbus_rtu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_rtu_parse(data, len, &frame);

    // Explicit CRC check
    modbus_rtu_check_crc(data, len);

    return 0;
}
