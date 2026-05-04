/*
 * prop_modbus_rtu.c — property-based tests for modbus_rtu
 *
 * Build: gcc -std=c99 -I.. -I../../../primitives/crc16 -I../../../../tooling/test \
 *        prop_modbus_rtu.c ../modbus_rtu.c ../../../primitives/crc16/crc16.c -o prop_modbus_rtu
 */

#include "test_harness.h"
#include "property_test.h"
#include "../modbus_rtu.h"
#include <string.h>

/* Property: build then parse roundtrip preserves addr, func, data */
PROPERTY(build_parse_roundtrip, 10000) {
    uint8_t addr = ARBI_UINT8(seed) | 1u; /* 1..255, avoid broadcast */
    uint8_t func = (uint8_t)ARBI_RANGE(seed, 1, 127); /* valid function codes */
    uint8_t data[200];
    uint16_t data_len = (uint16_t)ARBI_RANGE(seed, 0, 200);
    ARBI_FILL(seed, data, data_len);

    uint8_t buf[256];
    int built = modbus_rtu_build(buf, sizeof(buf), addr, func, data, data_len);
    PROP_ASSERT(built > 0);

    /* CRC must validate */
    PROP_ASSERT(modbus_rtu_check_crc(buf, (uint16_t)built));

    modbus_rtu_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    int rc = modbus_rtu_parse(buf, (uint16_t)built, &frame);
    PROP_ASSERT_EQ(rc, 0);
    PROP_ASSERT_EQ(frame.addr, addr);
    PROP_ASSERT_EQ(frame.func, func);
    PROP_ASSERT_EQ(frame.data_len, data_len);
    if (data_len > 0) {
        PROP_ASSERT_MEM_EQ(frame.data, data, data_len);
    }
}

/* Property: read registers request roundtrip */
PROPERTY(read_regs_roundtrip, 10000) {
    uint8_t slave = (uint8_t)ARBI_RANGE(seed, 1, 247);
    uint8_t func = (ARBI_UINT8(seed) & 1)
                       ? MODBUS_FC_READ_HOLDING_REGS
                       : MODBUS_FC_READ_INPUT_REGS;
    uint16_t start = ARBI_UINT16(seed);
    uint16_t count = (uint16_t)ARBI_RANGE(seed, 1, MODBUS_MAX_READ_REGS);

    uint8_t buf[256];
    int built = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                            slave, func, start, count);
    PROP_ASSERT(built > 0);
    PROP_ASSERT(modbus_rtu_check_crc(buf, (uint16_t)built));

    modbus_rtu_frame_t frame;
    int rc = modbus_rtu_parse(buf, (uint16_t)built, &frame);
    PROP_ASSERT_EQ(rc, 0);
    PROP_ASSERT_EQ(frame.addr, slave);
    PROP_ASSERT_EQ(frame.func, func);

    /* Data should be 4 bytes: start_hi, start_lo, count_hi, count_lo */
    PROP_ASSERT_EQ(frame.data_len, 4);
    uint16_t parsed_start = (uint16_t)((uint16_t)frame.data[0] << 8 | frame.data[1]);
    uint16_t parsed_count = (uint16_t)((uint16_t)frame.data[2] << 8 | frame.data[3]);
    PROP_ASSERT_EQ(parsed_start, start);
    PROP_ASSERT_EQ(parsed_count, count);
}

/* Property: write single register request preserves reg + value */
PROPERTY(write_reg_roundtrip, 10000) {
    uint8_t slave = (uint8_t)ARBI_RANGE(seed, 1, 247);
    uint16_t reg = ARBI_UINT16(seed);
    uint16_t value = ARBI_UINT16(seed);

    uint8_t buf[256];
    int built = modbus_rtu_master_write_reg(buf, sizeof(buf),
                                            slave, reg, value);
    PROP_ASSERT(built > 0);
    PROP_ASSERT(modbus_rtu_check_crc(buf, (uint16_t)built));

    modbus_rtu_frame_t frame;
    int rc = modbus_rtu_parse(buf, (uint16_t)built, &frame);
    PROP_ASSERT_EQ(rc, 0);
    PROP_ASSERT_EQ(frame.addr, slave);
    PROP_ASSERT_EQ(frame.func, MODBUS_FC_WRITE_SINGLE_REG);
    PROP_ASSERT_EQ(frame.data_len, 4);
    uint16_t parsed_reg = (uint16_t)((uint16_t)frame.data[0] << 8 | frame.data[1]);
    uint16_t parsed_val = (uint16_t)((uint16_t)frame.data[2] << 8 | frame.data[3]);
    PROP_ASSERT_EQ(parsed_reg, reg);
    PROP_ASSERT_EQ(parsed_val, value);
}

/* Property: build is deterministic */
PROPERTY(build_determinism, 5000) {
    uint8_t addr = ARBI_UINT8(seed);
    uint8_t func = ARBI_UINT8(seed);
    uint8_t data[50];
    uint16_t dlen = (uint16_t)ARBI_RANGE(seed, 0, 50);
    ARBI_FILL(seed, data, dlen);

    uint8_t buf1[256], buf2[256];
    int len1 = modbus_rtu_build(buf1, sizeof(buf1), addr, func, data, dlen);
    int len2 = modbus_rtu_build(buf2, sizeof(buf2), addr, func, data, dlen);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[300];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 256);
    ARBI_FILL(seed, garbage, len);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(garbage, len, &frame);
    modbus_rtu_check_crc(garbage, len);
}

TEST_MAIN("modbus_rtu properties") {
    RUN(build_parse_roundtrip);
    RUN(read_regs_roundtrip);
    RUN(write_reg_roundtrip);
    RUN(build_determinism);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
