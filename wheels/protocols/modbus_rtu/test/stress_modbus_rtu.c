// SPDX-License-Identifier: MIT
/*
 * stress_modbus_rtu.c — long-running stress tests for modbus_rtu
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra \
 *       -I.. -I../../../primitives/crc16 -I../../../../tooling/harness \
 *       stress_modbus_rtu.c ../modbus_rtu.c ../../../primitives/crc16/crc16.c \
 *       -o /tmp/stress_modbus_rtu -lm
 *
 * Scenarios:
 *   1. Build/parse roundtrip — 100K frames, verify CRC and data integrity
 *   2. Master read request cycle — varying slave/register over 100K iterations
 *   3. Slave process stress — sustained request/response cycles with mock registers
 *   4. CRC validation stress — verify check_crc on known-good frames at scale
 */

#include "test_harness.h"
#include "modbus_rtu.h"
#include <string.h>

/* ---- mock slave register bank ---- */

#define NUM_REGS 256
static uint16_t g_holding_regs[NUM_REGS];
static uint16_t g_input_regs[NUM_REGS];

static int mock_read_holding(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    if (addr >= NUM_REGS) return -1;
    *value = g_holding_regs[addr];
    return 0;
}

static int mock_read_input(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    if (addr >= NUM_REGS) return -1;
    *value = g_input_regs[addr];
    return 0;
}

static int mock_write_holding(void *ctx, uint16_t addr, uint16_t value)
{
    (void)ctx;
    if (addr >= NUM_REGS) return -1;
    g_holding_regs[addr] = value;
    return 0;
}

/* ---- tests ---------------------------------------------------------- */

TEST(build_parse_roundtrip_100k)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        uint8_t addr = (uint8_t)((i % 247) + 1); /* slave addr 1-247 */
        uint8_t func = MODBUS_FC_READ_HOLDING_REGS;
        uint8_t data[4];
        data[0] = (uint8_t)((i >> 8) & 0xFF);
        data[1] = (uint8_t)(i & 0xFF);
        data[2] = 0x00;
        data[3] = 0x01;

        int len = modbus_rtu_build(buf, sizeof(buf), addr, func, data, 4);
        if (len < MODBUS_RTU_MIN_FRAME) { ok = 0; break; }

        /* Verify CRC */
        if (!modbus_rtu_check_crc(buf, (uint16_t)len)) { ok = 0; break; }

        /* Parse back */
        modbus_rtu_frame_t frame;
        if (modbus_rtu_parse(buf, (uint16_t)len, &frame) != 0) { ok = 0; break; }
        if (frame.addr != addr) { ok = 0; break; }
        if (frame.func != func) { ok = 0; break; }
        if (frame.data_len != 4) { ok = 0; break; }
        if (memcmp(frame.data, data, 4) != 0) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(master_read_request_varying)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        uint8_t slave = (uint8_t)((i % 247) + 1);
        uint16_t start = (uint16_t)(i % 1000);
        uint16_t count = (uint16_t)((i % 10) + 1);
        uint8_t func = (i % 2 == 0) ? MODBUS_FC_READ_HOLDING_REGS
                                     : MODBUS_FC_READ_INPUT_REGS;

        int len = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                              slave, func, start, count);
        if (len <= 0) { ok = 0; break; }
        if (!modbus_rtu_check_crc(buf, (uint16_t)len)) { ok = 0; break; }

        /* Parse and verify fields */
        modbus_rtu_frame_t frame;
        if (modbus_rtu_parse(buf, (uint16_t)len, &frame) != 0) { ok = 0; break; }
        if (frame.addr != slave) { ok = 0; break; }
        if (frame.func != func) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(slave_process_sustained)
{
    /* Initialize slave and register bank */
    modbus_rtu_slave_t slave;
    modbus_rtu_slave_init(&slave, 1, NULL);
    slave.read_holding  = mock_read_holding;
    slave.read_input    = mock_read_input;
    slave.write_holding = mock_write_holding;

    for (int i = 0; i < NUM_REGS; i++) {
        g_holding_regs[i] = (uint16_t)(i * 10);
        g_input_regs[i]   = (uint16_t)(i * 20);
    }

    uint8_t req_buf[MODBUS_RTU_MAX_FRAME];
    uint8_t resp_buf[MODBUS_RTU_MAX_FRAME];
    int ok = 1;

    for (int i = 0; i < 50000; i++) {
        /* Build a read holding request for register (i % NUM_REGS), count 1 */
        uint16_t reg_addr = (uint16_t)(i % NUM_REGS);
        int req_len = modbus_rtu_master_read_regs(req_buf, sizeof(req_buf),
                                                  1, MODBUS_FC_READ_HOLDING_REGS,
                                                  reg_addr, 1);
        if (req_len <= 0) { ok = 0; break; }

        /* Process request */
        int resp_len = modbus_rtu_slave_process(&slave, req_buf, (uint16_t)req_len,
                                                resp_buf, sizeof(resp_buf));
        if (resp_len <= 0) { ok = 0; break; }

        /* Verify response CRC */
        if (!modbus_rtu_check_crc(resp_buf, (uint16_t)resp_len)) { ok = 0; break; }

        /* Extract register value from response */
        modbus_rtu_frame_t resp_frame;
        if (modbus_rtu_parse(resp_buf, (uint16_t)resp_len, &resp_frame) != 0) {
            ok = 0; break;
        }
        uint16_t regs[1];
        int got = modbus_rtu_master_get_regs(&resp_frame, regs, 1);
        if (got != 1) { ok = 0; break; }
        if (regs[0] != g_holding_regs[reg_addr]) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);

    /* Also stress write-single-reg + readback */
    for (int i = 0; i < 50000; i++) {
        uint16_t reg_addr = (uint16_t)(i % NUM_REGS);
        uint16_t value = (uint16_t)(i & 0xFFFF);

        int req_len = modbus_rtu_master_write_reg(req_buf, sizeof(req_buf),
                                                  1, reg_addr, value);
        if (req_len <= 0) { ok = 0; break; }

        int resp_len = modbus_rtu_slave_process(&slave, req_buf, (uint16_t)req_len,
                                                resp_buf, sizeof(resp_buf));
        if (resp_len <= 0) { ok = 0; break; }
        if (!modbus_rtu_check_crc(resp_buf, (uint16_t)resp_len)) { ok = 0; break; }

        /* Verify register was actually written */
        if (g_holding_regs[reg_addr] != value) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
}

TEST(crc_validation_known_good)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME];
    int ok = 1;

    for (int i = 0; i < 100000; i++) {
        /* Build a valid frame */
        uint8_t data[2] = { (uint8_t)(i & 0xFF), (uint8_t)((i >> 8) & 0xFF) };
        int len = modbus_rtu_build(buf, sizeof(buf), 1, 0x03, data, 2);
        if (len <= 0) { ok = 0; break; }

        /* CRC must pass */
        if (!modbus_rtu_check_crc(buf, (uint16_t)len)) { ok = 0; break; }

        /* Corrupt one byte and verify CRC fails */
        uint8_t saved = buf[2];
        buf[2] ^= 0xFF;
        if (modbus_rtu_check_crc(buf, (uint16_t)len)) { ok = 0; break; }
        buf[2] = saved; /* restore */
    }
    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress modbus_rtu")
    RUN(build_parse_roundtrip_100k);
    RUN(master_read_request_varying);
    RUN(slave_process_sustained);
    RUN(crc_validation_known_good);
TEST_MAIN_END()
