// SPDX-License-Identifier: MIT
// LibFuzzer harness for modbus_tcp -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_modbus_tcp.c ../modbus_tcp.c -o fuzz_modbus_tcp
// Run:     ./fuzz_modbus_tcp -max_total_time=60 corpus_modbus_tcp/

#include "../modbus_tcp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);

    // Parse frame from fuzz data
    modbus_tcp_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_tcp_err_t err = modbus_tcp_parse(data, len, &frame);

    // If parse succeeded, exercise response helpers
    if (err == MODBUS_TCP_OK) {
        // Check exception status
        modbus_tcp_is_exception(&frame);
        modbus_tcp_exception_code(&frame);

        // Try extracting registers (for FC 0x03 responses)
        uint16_t regs[MODBUS_TCP_MAX_READ_REGS];
        uint16_t actual = 0;
        modbus_tcp_get_regs(&frame, regs, MODBUS_TCP_MAX_READ_REGS, &actual);
    }

    // Exercise builder functions with data-derived params
    uint8_t build_buf[MODBUS_TCP_MAX_FRAME];

    if (size >= 6) {
        uint16_t tid = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
        uint8_t uid = data[2];
        uint16_t addr = (uint16_t)(data[3] | ((uint16_t)data[4] << 8));
        uint16_t count = (uint16_t)(data[5] % (MODBUS_TCP_MAX_READ_REGS + 1));

        modbus_tcp_build_read_regs(build_buf, sizeof(build_buf),
                                   tid, uid, addr, count);
    }

    if (size >= 6) {
        uint16_t tid = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
        uint8_t uid = data[2];
        uint16_t addr = (uint16_t)(data[3] | ((uint16_t)data[4] << 8));
        uint16_t value = (uint16_t)(data[5] | ((uint16_t)(size > 6 ? data[6] : 0) << 8));

        modbus_tcp_build_write_reg(build_buf, sizeof(build_buf),
                                   tid, uid, addr, value);
    }

    // Exercise write multiple with data-derived register values
    if (size >= 8) {
        uint16_t tid = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
        uint8_t uid = data[2];
        uint16_t addr = (uint16_t)(data[3] | ((uint16_t)data[4] << 8));
        uint16_t count = (uint16_t)((data[5] % MODBUS_TCP_MAX_WRITE_REGS) + 1);

        // Build register values from remaining fuzz data
        uint16_t values[MODBUS_TCP_MAX_WRITE_REGS];
        memset(values, 0, sizeof(values));
        size_t vals_avail = (size - 6) / 2;
        if (vals_avail > count) vals_avail = count;
        for (size_t i = 0; i < vals_avail; i++) {
            values[i] = (uint16_t)(data[6 + i * 2] |
                         ((uint16_t)data[6 + i * 2 + 1] << 8));
        }

        modbus_tcp_build_write_regs(build_buf, sizeof(build_buf),
                                    tid, uid, addr, values, count);
    }

    return 0;
}
