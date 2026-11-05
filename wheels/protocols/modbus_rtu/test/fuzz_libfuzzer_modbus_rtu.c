// SPDX-License-Identifier: MIT
// LibFuzzer harness for modbus_rtu — coverage-guided fuzzing
//
// Raw input stops at the CRC check, so the harness also appends a correct CRC
// and runs the frame through the slave and the response decoder.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../primitives/crc16 \
//          -I../../../../tooling/harness fuzz_libfuzzer_modbus_rtu.c ../modbus_rtu.c \
//          ../../../primitives/crc16/crc16.c -o fuzz_modbus_rtu
// Run:     ./fuzz_modbus_rtu -max_total_time=60 corpus_modbus_rtu/

#include "../modbus_rtu.h"
#include "crc16.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SLAVE_ADDR 1

static int read_reg(void *ctx, uint16_t addr, uint16_t *value) {
    (void)ctx;
    *value = (uint16_t)~addr;
    return (addr & 0xFF) == 0xA5 ? -1 : 0;
}

static int write_reg(void *ctx, uint16_t addr, uint16_t value) {
    (void)ctx;
    return (addr & 0xFF) == 0x5A && value != 0 ? -1 : 0;
}

// Everything the component builds has to parse back as a valid frame.
static void check_built(const uint8_t *buf, int len) {
    modbus_rtu_frame_t frame;
    if (len > 0 && modbus_rtu_parse(buf, (uint16_t)len, &frame) != 0) {
        __builtin_trap();
    }
}

static void fuzz_slave(const uint8_t *wire, uint16_t wire_len, uint8_t cfg) {
    modbus_rtu_slave_t slave;
    modbus_rtu_slave_init(&slave, SLAVE_ADDR, NULL);
    if (cfg & 0x01) slave.read_holding = read_reg;
    if (cfg & 0x02) slave.read_input = read_reg;
    if (cfg & 0x04) slave.write_holding = write_reg;

    // Exact-size response buffer, so ASan sees any write past resp_size.
    uint16_t resp_size = (cfg & 0x08) ? MODBUS_RTU_MAX_FRAME : (uint16_t)(cfg >> 4);
    uint8_t *resp = malloc(resp_size ? resp_size : 1);
    if (!resp) return;

    int rc = modbus_rtu_slave_process(&slave, wire, wire_len, resp, resp_size);
    check_built(resp, rc);
    free(resp);
}

static void fuzz_response(const uint8_t *wire, uint16_t wire_len, uint8_t cfg) {
    modbus_rtu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    if (modbus_rtu_parse(wire, wire_len, &frame) != 0) return;

    (void)modbus_rtu_is_exception(&frame);
    (void)modbus_rtu_exception_code(&frame);

    // Exact-size output, so ASan sees any write past max_count.
    uint16_t max_count = (uint16_t)(cfg % (MODBUS_MAX_READ_REGS + 1));
    uint16_t *regs = malloc((max_count ? max_count : 1) * sizeof(*regs));
    if (!regs) return;
    modbus_rtu_master_get_regs(&frame, regs, max_count);
    free(regs);
}

static void fuzz_master(fuzz_input_t *in) {
    uint8_t out[MODBUS_RTU_MAX_FRAME];
    uint16_t values[MODBUS_MAX_WRITE_REGS];
    uint8_t slave = fuzz_u8(in);
    uint8_t func = fuzz_u8(in);
    uint16_t start = fuzz_u16(in);
    uint16_t count = fuzz_u16(in);

    memset(values, 0, sizeof(values));
    fuzz_take(in, (uint8_t *)values, sizeof(values));

    check_built(out, modbus_rtu_master_read_regs(out, sizeof(out), slave, func,
                                                 start, count));
    check_built(out, modbus_rtu_master_write_reg(out, sizeof(out), slave, start,
                                                 count));
    check_built(out, modbus_rtu_master_write_regs(out, sizeof(out), slave, start,
                                                  count, values));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    // Raw bytes reach the length and CRC rejection paths.
    modbus_rtu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_rtu_parse(data, len, &frame);
    modbus_rtu_check_crc(data, len);

    if (size == 0) return 0;

    // First byte configures the run, the rest is the frame body.
    uint8_t cfg = data[0];
    size_t body_len = size - 1;
    if (body_len > MODBUS_RTU_MAX_FRAME - 2) body_len = MODBUS_RTU_MAX_FRAME - 2;

    // Exact-size frame, so ASan sees any read past the CRC.
    uint8_t *wire = malloc(body_len + 2);
    if (!wire) return 0;
    memcpy(wire, data + 1, body_len);
    uint16_t crc = crc16_modbus(wire, (uint16_t)body_len);
    wire[body_len] = (uint8_t)(crc & 0xFF);
    wire[body_len + 1] = (uint8_t)(crc >> 8);

    fuzz_slave(wire, (uint16_t)(body_len + 2), cfg);
    fuzz_response(wire, (uint16_t)(body_len + 2), cfg);
    free(wire);

    fuzz_input_t in;
    fuzz_input_init(&in, data + 1, size - 1);
    fuzz_master(&in);

    return 0;
}
