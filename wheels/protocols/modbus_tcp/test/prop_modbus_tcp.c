/*
 * prop_modbus_tcp.c — property-based tests for modbus_tcp
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_modbus_tcp.c ../modbus_tcp.c -o prop_modbus_tcp
 */

#include "test_harness.h"
#include "property_test.h"
#include "../modbus_tcp.h"
#include <string.h>

/* Property: build read_regs then parse roundtrip */
PROPERTY(read_regs_roundtrip, 10000) {
    uint16_t tid = ARBI_UINT16(seed);
    uint8_t uid = ARBI_UINT8(seed);
    uint16_t start = ARBI_UINT16(seed);
    uint16_t count = (uint16_t)ARBI_RANGE(seed, 1, MODBUS_TCP_MAX_READ_REGS);

    uint8_t buf[MODBUS_TCP_MAX_FRAME];
    int built = modbus_tcp_build_read_regs(buf, sizeof(buf), tid, uid, start, count);
    PROP_ASSERT(built > 0);

    modbus_tcp_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_tcp_err_t err = modbus_tcp_parse(buf, (uint16_t)built, &frame);
    PROP_ASSERT_EQ(err, MODBUS_TCP_OK);
    PROP_ASSERT_EQ(frame.header.transaction_id, tid);
    PROP_ASSERT_EQ(frame.header.unit_id, uid);
    PROP_ASSERT_EQ(frame.function_code, MODBUS_TCP_FC_READ_HOLDING);
}

/* Property: write_reg build is deterministic */
PROPERTY(write_reg_determinism, 5000) {
    uint16_t tid = ARBI_UINT16(seed);
    uint8_t uid = ARBI_UINT8(seed);
    uint16_t addr = ARBI_UINT16(seed);
    uint16_t val = ARBI_UINT16(seed);

    uint8_t buf1[MODBUS_TCP_MAX_FRAME], buf2[MODBUS_TCP_MAX_FRAME];
    int len1 = modbus_tcp_build_write_reg(buf1, sizeof(buf1), tid, uid, addr, val);
    int len2 = modbus_tcp_build_write_reg(buf2, sizeof(buf2), tid, uid, addr, val);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: write_multiple build then parse preserves header fields */
PROPERTY(write_multi_roundtrip, 5000) {
    uint16_t tid = ARBI_UINT16(seed);
    uint8_t uid = ARBI_UINT8(seed);
    uint16_t start = ARBI_UINT16(seed);
    uint16_t count = (uint16_t)ARBI_RANGE(seed, 1, 10);
    uint16_t values[10];
    for (uint16_t i = 0; i < count; i++)
        values[i] = ARBI_UINT16(seed);

    uint8_t buf[MODBUS_TCP_MAX_FRAME];
    int built = modbus_tcp_build_write_regs(buf, sizeof(buf), tid, uid, start, values, count);
    PROP_ASSERT(built > 0);

    modbus_tcp_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_tcp_err_t err = modbus_tcp_parse(buf, (uint16_t)built, &frame);
    PROP_ASSERT_EQ(err, MODBUS_TCP_OK);
    PROP_ASSERT_EQ(frame.header.transaction_id, tid);
    PROP_ASSERT_EQ(frame.header.unit_id, uid);
    PROP_ASSERT_EQ(frame.function_code, MODBUS_TCP_FC_WRITE_MULTIPLE);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[MODBUS_TCP_MAX_FRAME];
    size_t len = ARBI_SIZE(seed, MODBUS_TCP_MAX_FRAME);
    ARBI_FILL(seed, garbage, len);

    modbus_tcp_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    modbus_tcp_parse(garbage, (uint16_t)len, &frame);

    /* Also try get_regs on any parsed frame */
    uint16_t regs[MODBUS_TCP_MAX_READ_REGS];
    uint16_t actual = 0;
    modbus_tcp_get_regs(&frame, regs, MODBUS_TCP_MAX_READ_REGS, &actual);
}

TEST_MAIN("modbus_tcp properties") {
    RUN(read_regs_roundtrip);
    RUN(write_reg_determinism);
    RUN(write_multi_roundtrip);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
