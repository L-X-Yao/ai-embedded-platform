// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "modbus_rtu.h"

/* ---------- test harness ---------- */
/* ---------- mock register bank for slave tests ---------- */

#define REG_BANK_SIZE 32
static uint16_t holding_regs[REG_BANK_SIZE];
static uint16_t input_regs[REG_BANK_SIZE];
static int      write_count;
static int      fail_next_read;
static int      fail_next_write;

static void mock_reset(void)
{
    memset(holding_regs, 0, sizeof(holding_regs));
    memset(input_regs, 0, sizeof(input_regs));
    write_count = 0;
    fail_next_read = 0;
    fail_next_write = 0;
}

static int mock_read_holding(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    if (fail_next_read) { fail_next_read = 0; return -1; }
    if (addr >= REG_BANK_SIZE) return -1;
    *value = holding_regs[addr];
    return 0;
}

static int mock_read_input(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    if (fail_next_read) { fail_next_read = 0; return -1; }
    if (addr >= REG_BANK_SIZE) return -1;
    *value = input_regs[addr];
    return 0;
}

static int mock_write_holding(void *ctx, uint16_t addr, uint16_t value)
{
    (void)ctx;
    if (fail_next_write) { fail_next_write = 0; return -1; }
    if (addr >= REG_BANK_SIZE) return -1;
    holding_regs[addr] = value;
    write_count++;
    return 0;
}

/* ============================================================ */
/*                    FRAME LAYER TESTS                         */
/* ============================================================ */

TEST(test_build_and_check_crc)
{
    uint8_t buf[32];
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT_TRUE(len == 4);
    ASSERT_TRUE(modbus_rtu_check_crc(buf, (uint16_t)len));

    buf[len - 1] ^= 0xFF;
    ASSERT_TRUE(!modbus_rtu_check_crc(buf, (uint16_t)len));
}

TEST(test_build_with_data)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT_TRUE(len == 8);
    ASSERT_TRUE(buf[0] == 0x01);
    ASSERT_TRUE(buf[1] == 0x03);
    ASSERT_TRUE(buf[2] == 0x00 && buf[3] == 0x6B);
    ASSERT_TRUE(modbus_rtu_check_crc(buf, (uint16_t)len));
}

TEST(test_build_buffer_too_small)
{
    uint8_t buf[4];
    ASSERT_EQ(modbus_rtu_build(buf, 3, 1, 3, NULL, 0), MODBUS_RTU_ERR_OVERFLOW);
    ASSERT_EQ(modbus_rtu_build(NULL, 32, 1, 3, NULL, 0), MODBUS_RTU_ERR_PARAM);
}

TEST(test_parse_valid)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.addr == 0x01);
    ASSERT_TRUE(frame.func == 0x03);
    ASSERT_TRUE(frame.data_len == 4);
    ASSERT_TRUE(frame.data[0] == 0x00 && frame.data[1] == 0x6B);
}

TEST(test_parse_minimal)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.data == NULL);
    ASSERT_TRUE(frame.data_len == 0);
}

TEST(test_parse_bad_crc)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    buf[len - 1] ^= 0xFF;
    ASSERT_EQ(modbus_rtu_parse(buf, (uint16_t)len, &frame), MODBUS_RTU_ERR_CRC);
}

TEST(test_parse_too_short)
{
    uint8_t buf[] = {0x01, 0x03, 0x00};
    modbus_rtu_frame_t frame;
    ASSERT_EQ(modbus_rtu_parse(buf, 3, &frame), MODBUS_RTU_ERR_OVERFLOW);
    ASSERT_EQ(modbus_rtu_parse(buf, 0, &frame), MODBUS_RTU_ERR_OVERFLOW);
}

TEST(test_parse_null_safety)
{
    uint8_t buf[8];
    modbus_rtu_frame_t frame;
    ASSERT_TRUE(modbus_rtu_parse(NULL, 8, &frame) == -1);
    ASSERT_TRUE(modbus_rtu_parse(buf, 8, NULL) == -1);
    ASSERT_TRUE(!modbus_rtu_check_crc(NULL, 8));
    ASSERT_TRUE(!modbus_rtu_check_crc(buf, 2));
}

TEST(test_exception_helpers)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;
    uint8_t ex_data = MODBUS_EX_ILLEGAL_FUNCTION;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x83, &ex_data, 1);
    modbus_rtu_parse(buf, (uint16_t)len, &frame);

    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_FUNCTION);

    len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    modbus_rtu_parse(buf, (uint16_t)len, &frame);
    ASSERT_TRUE(!modbus_rtu_is_exception(&frame));

    ASSERT_TRUE(!modbus_rtu_is_exception(NULL));
    ASSERT_TRUE(modbus_rtu_exception_code(NULL) == 0);
}

/* ============================================================ */
/*                    MASTER TESTS                              */
/* ============================================================ */

TEST(test_master_read_holding)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                          0x01, MODBUS_FC_READ_HOLDING_REGS,
                                          0x006B, 3);
    ASSERT_TRUE(len == 8);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.addr == 0x01);
    ASSERT_TRUE(frame.func == 0x03);
    ASSERT_TRUE(frame.data[0] == 0x00 && frame.data[1] == 0x6B);
    ASSERT_TRUE(frame.data[2] == 0x00 && frame.data[3] == 0x03);
}

TEST(test_master_read_input)
{
    uint8_t buf[32];
    int len = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                          0x02, MODBUS_FC_READ_INPUT_REGS,
                                          0x0000, 1);
    ASSERT_TRUE(len == 8);
    ASSERT_TRUE(buf[1] == 0x04);
}

TEST(test_master_read_invalid)
{
    uint8_t buf[32];
    ASSERT_EQ(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x01, 0, 1), MODBUS_RTU_ERR_PARAM);
    ASSERT_EQ(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x03, 0, 0), MODBUS_RTU_ERR_OVERFLOW);
    ASSERT_EQ(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x03, 0, 126), MODBUS_RTU_ERR_OVERFLOW);
}

TEST(test_master_write_single)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_master_write_reg(buf, sizeof(buf),
                                          0x01, 0x0001, 0x0003);
    ASSERT_TRUE(len == 8);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.func == 0x06);
    ASSERT_TRUE(frame.data[0] == 0x00 && frame.data[1] == 0x01);
    ASSERT_TRUE(frame.data[2] == 0x00 && frame.data[3] == 0x03);
}

TEST(test_master_write_multiple)
{
    uint8_t buf[64];
    modbus_rtu_frame_t frame;
    uint16_t values[] = {0x000A, 0x0102};
    int len = modbus_rtu_master_write_regs(buf, sizeof(buf),
                                           0x01, 0x0001, 2, values);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.func == 0x10);
    ASSERT_TRUE(frame.data[4] == 4);
    ASSERT_TRUE(frame.data[5] == 0x00 && frame.data[6] == 0x0A);
    ASSERT_TRUE(frame.data[7] == 0x01 && frame.data[8] == 0x02);
}

TEST(test_master_write_multiple_invalid)
{
    uint8_t buf[64];
    uint16_t v = 1;
    ASSERT_EQ(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0, 0, &v), MODBUS_RTU_ERR_OVERFLOW);
    ASSERT_EQ(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0, 1, NULL), MODBUS_RTU_ERR_PARAM);
    ASSERT_EQ(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0,
           MODBUS_MAX_WRITE_REGS + 1, &v), MODBUS_RTU_ERR_OVERFLOW);
}

TEST(test_master_get_regs)
{
    /* Build a fake read response: addr=1, FC=03, byte_count=4, regs=[0x0064, 0x00C8] */
    uint8_t resp_buf[32];
    uint8_t resp_data[] = {0x04, 0x00, 0x64, 0x00, 0xC8};
    int len = modbus_rtu_build(resp_buf, sizeof(resp_buf), 0x01, 0x03,
                               resp_data, 5);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp_buf, (uint16_t)len, &frame);

    uint16_t regs[4];
    int n = modbus_rtu_master_get_regs(&frame, regs, 4);
    ASSERT_TRUE(n == 2);
    ASSERT_TRUE(regs[0] == 0x0064);
    ASSERT_TRUE(regs[1] == 0x00C8);
}

TEST(test_master_get_regs_truncate)
{
    uint8_t resp_buf[32];
    uint8_t resp_data[] = {0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
    int len = modbus_rtu_build(resp_buf, sizeof(resp_buf), 0x01, 0x03,
                               resp_data, 7);
    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp_buf, (uint16_t)len, &frame);

    uint16_t regs[2];
    int n = modbus_rtu_master_get_regs(&frame, regs, 2);
    ASSERT_TRUE(n == 2);
    ASSERT_TRUE(regs[0] == 1 && regs[1] == 2);
}

TEST(test_master_get_regs_invalid)
{
    uint16_t regs[4];

    ASSERT_EQ(modbus_rtu_master_get_regs(NULL, regs, 4), MODBUS_RTU_ERR_PARAM);

    modbus_rtu_frame_t frame = {0};
    ASSERT_EQ(modbus_rtu_master_get_regs(&frame, regs, 4), MODBUS_RTU_ERR_PARAM);

    /* Odd byte count */
    uint8_t buf[32];
    uint8_t bad_data[] = {0x03, 0x00, 0x01, 0x00};
    int len = modbus_rtu_build(buf, sizeof(buf), 1, 3, bad_data, 4);
    modbus_rtu_parse(buf, (uint16_t)len, &frame);
    ASSERT_EQ(modbus_rtu_master_get_regs(&frame, regs, 4), MODBUS_RTU_ERR_OVERFLOW);
}

/* ============================================================ */
/*                    SLAVE TESTS                               */
/* ============================================================ */

TEST(test_slave_init)
{
    modbus_rtu_slave_t s;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    ASSERT_TRUE(s.addr == 0x01);
    ASSERT_TRUE(s.read_holding == NULL);
    ASSERT_TRUE(s.ctx == NULL);

    modbus_rtu_slave_init(NULL, 1, NULL);  /* should not crash */
    ASSERT_TRUE(1);
}

TEST(test_slave_read_holding)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    holding_regs[0] = 0x1234;
    holding_regs[1] = 0x5678;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 2);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    ASSERT_TRUE(modbus_rtu_parse(resp, (uint16_t)resp_len, &frame) == 0);
    ASSERT_TRUE(frame.func == 0x03);

    uint16_t regs[2];
    int n = modbus_rtu_master_get_regs(&frame, regs, 2);
    ASSERT_TRUE(n == 2);
    ASSERT_TRUE(regs[0] == 0x1234);
    ASSERT_TRUE(regs[1] == 0x5678);
}

TEST(test_slave_read_input)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    input_regs[5] = 0xABCD;

    modbus_rtu_slave_init(&s, 0x02, NULL);
    s.read_input = mock_read_input;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x02, MODBUS_FC_READ_INPUT_REGS,
                                               5, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    uint16_t val;
    modbus_rtu_master_get_regs(&frame, &val, 1);
    ASSERT_TRUE(val == 0xABCD);
}

TEST(test_slave_write_single)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x01, 3, 0x00FF);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);
    ASSERT_TRUE(holding_regs[3] == 0x00FF);
    ASSERT_TRUE(write_count == 1);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(frame.func == 0x06);
}

TEST(test_slave_write_multiple)
{
    modbus_rtu_slave_t s;
    uint8_t req[64], resp[32];
    uint16_t values[] = {0x000A, 0x000B, 0x000C};
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_regs(req, sizeof(req),
                                                0x01, 10, 3, values);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);
    ASSERT_TRUE(holding_regs[10] == 0x000A);
    ASSERT_TRUE(holding_regs[11] == 0x000B);
    ASSERT_TRUE(holding_regs[12] == 0x000C);
    ASSERT_TRUE(write_count == 3);
}

TEST(test_slave_address_mismatch)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x02, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len == 0);
}

TEST(test_slave_broadcast_write)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x00, 0, 0x1234);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len == 0);
    ASSERT_TRUE(holding_regs[0] == 0x1234);
}

TEST(test_slave_broadcast_read_ignored)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x00, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len == 0);
}

TEST(test_slave_unknown_function)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    modbus_rtu_slave_init(&s, 0x01, NULL);

    int req_len = modbus_rtu_build(req, sizeof(req), 0x01, 0x01, NULL, 0);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_FUNCTION);
}

TEST(test_slave_no_callback)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    modbus_rtu_slave_init(&s, 0x01, NULL);
    /* read_holding not set */

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
}

TEST(test_slave_callback_failure)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    fail_next_read = 1;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(modbus_rtu_exception_code(&frame) == MODBUS_EX_SLAVE_FAILURE);
}

TEST(test_slave_write_callback_failure)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    mock_reset();
    fail_next_write = 1;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req), 0x01, 0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
}

TEST(test_slave_null_safety)
{
    uint8_t req[32] = {0}, resp[32] = {0};
    ASSERT_TRUE(modbus_rtu_slave_process(NULL, req, 8, resp, 32) == -1);

    modbus_rtu_slave_t s;
    modbus_rtu_slave_init(&s, 1, NULL);
    ASSERT_TRUE(modbus_rtu_slave_process(&s, NULL, 8, resp, 32) == -1);
    ASSERT_TRUE(modbus_rtu_slave_process(&s, req, 8, NULL, 32) == -1);
}

/* ============================================================ */
/*                    ROUND-TRIP INTEGRATION                    */
/* ============================================================ */

TEST(test_roundtrip_read)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    holding_regs[0] = 100;
    holding_regs[1] = 200;
    holding_regs[2] = 300;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 3);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);

    uint16_t regs[3];
    int n = modbus_rtu_master_get_regs(&frame, regs, 3);
    ASSERT_TRUE(n == 3);
    ASSERT_TRUE(regs[0] == 100);
    ASSERT_TRUE(regs[1] == 200);
    ASSERT_TRUE(regs[2] == 300);
}

TEST(test_roundtrip_write_single)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x01, 5, 0xBEEF);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(holding_regs[5] == 0xBEEF);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(!modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(frame.func == 0x06);
}

TEST(test_roundtrip_write_multiple)
{
    modbus_rtu_slave_t s;
    uint8_t req[64], resp[32];
    uint16_t values[] = {0x1111, 0x2222};
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_regs(req, sizeof(req),
                                                0x01, 0, 2, values);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(holding_regs[0] == 0x1111);
    ASSERT_TRUE(holding_regs[1] == 0x2222);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(!modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(frame.func == 0x10);
}

/* ============================================================ */
/*                    BOUNDARY TESTS                           */
/* ============================================================ */

TEST(test_build_max_data)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME];
    uint8_t data[MODBUS_RTU_MAX_FRAME - 4];
    memset(data, 0xAA, sizeof(data));
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03,
                               data, MODBUS_RTU_MAX_FRAME - 4);
    ASSERT_TRUE(len == MODBUS_RTU_MAX_FRAME);
    ASSERT_TRUE(modbus_rtu_check_crc(buf, (uint16_t)len));

    modbus_rtu_frame_t frame;
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.data_len == MODBUS_RTU_MAX_FRAME - 4);
}

TEST(test_build_exceeds_max_frame)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME + 4];
    uint8_t data[MODBUS_RTU_MAX_FRAME - 3];
    memset(data, 0xBB, sizeof(data));
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03,
                               data, MODBUS_RTU_MAX_FRAME - 3);
    ASSERT_EQ(len, MODBUS_RTU_ERR_OVERFLOW);
}

TEST(test_slave_reinit_address)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    holding_regs[0] = 0xAAAA;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    ASSERT_TRUE(modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp)) > 0);

    modbus_rtu_slave_init(&s, 0x05, NULL);
    s.read_holding = mock_read_holding;

    ASSERT_TRUE(modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp)) == 0);

    req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                           0x05, MODBUS_FC_READ_HOLDING_REGS,
                                           0, 1);
    ASSERT_TRUE(modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp)) > 0);
}

TEST(test_crc_corruption_middle_byte)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT_TRUE(len == 8);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);

    buf[3] ^= 0xFF;
    ASSERT_EQ(modbus_rtu_parse(buf, (uint16_t)len, &frame), MODBUS_RTU_ERR_CRC);

    buf[3] ^= 0xFF;
    buf[0] ^= 0x01;
    ASSERT_EQ(modbus_rtu_parse(buf, (uint16_t)len, &frame), MODBUS_RTU_ERR_CRC);
}

TEST(test_build_null_data_nonzero_len)
{
    uint8_t buf[32];
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 4);
    ASSERT_EQ(len, -1);

    len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT_EQ(len, 4);
    ASSERT_TRUE(modbus_rtu_check_crc(buf, (uint16_t)len));
}

TEST(test_slave_resp_buffer_too_small)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[4];
    mock_reset();
    holding_regs[0] = 0x1234;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_EQ(resp_len, MODBUS_RTU_ERR_OVERFLOW);
}

TEST(test_slave_write_multiple_mismatched_count)
{
    modbus_rtu_slave_t s;
    uint8_t raw[32], resp[64];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    uint8_t bad_data[] = {
        0x00, 0x00,  /* start = 0 */
        0x00, 0x02,  /* count = 2 */
        0x02,        /* byte_count = 2 (should be 4 for 2 regs) */
        0x00, 0x0A   /* only 1 register value */
    };
    int raw_len = modbus_rtu_build(raw, sizeof(raw), 0x01,
                                    MODBUS_FC_WRITE_MULTIPLE_REGS,
                                    bad_data, 7);
    int resp_len = modbus_rtu_slave_process(&s, raw, (uint16_t)raw_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(test_slave_read_max_regs)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[512];
    mock_reset();
    for (int i = 0; i < REG_BANK_SIZE; i++)
        holding_regs[i] = (uint16_t)(i + 1);

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, REG_BANK_SIZE);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(!modbus_rtu_is_exception(&frame));

    uint16_t regs[REG_BANK_SIZE];
    int n = modbus_rtu_master_get_regs(&frame, regs, REG_BANK_SIZE);
    ASSERT_TRUE(n == REG_BANK_SIZE);
    ASSERT_TRUE(regs[0] == 1);
    ASSERT_TRUE(regs[REG_BANK_SIZE - 1] == REG_BANK_SIZE);
}

TEST(test_build_data_len_overflow)
{
    uint8_t buf[512] = {0};
    static const uint8_t data[MODBUS_RTU_MAX_FRAME] = {0};
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 0xFFFF);
    ASSERT_EQ(len, MODBUS_RTU_ERR_OVERFLOW);

    len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, MODBUS_RTU_MAX_FRAME - 3);
    ASSERT_EQ(len, MODBUS_RTU_ERR_OVERFLOW);

    len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, MODBUS_RTU_MAX_FRAME - 4);
    ASSERT_TRUE(len == MODBUS_RTU_MAX_FRAME);
}

TEST(test_parse_exact_min_frame)
{
    uint8_t buf[4];
    modbus_rtu_frame_t frame;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT_TRUE(len == 4);
    ASSERT_TRUE(modbus_rtu_parse(buf, (uint16_t)len, &frame) == 0);
    ASSERT_TRUE(frame.data == NULL);
    ASSERT_TRUE(frame.data_len == 0);
    ASSERT_TRUE(frame.addr == 0x01);
    ASSERT_TRUE(frame.func == 0x03);
}

TEST(test_slave_address_wrap)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0xFFFF, 2);
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));
    ASSERT_TRUE(resp_len > 0);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, (uint16_t)resp_len, &frame);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_TRUE(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* ============================================================ */

/* ============================================================ */
/*             WHOLE RESPONSES AND THE MAXIMUM READ             */
/* ============================================================ */

static int accept_any_write(void *ctx, uint16_t addr, uint16_t value)
{
    (void)ctx;
    (void)addr;
    (void)value;
    return 0;
}

/* FC16 answers with the slave, function, start and count of the request. CRC
 * values here come from CRC-16/MODBUS worked out apart from this code. */
TEST(test_slave_write_multiple_whole_response)
{
    static const uint8_t req[] = {
        0x11, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04,
        0x00, 0x0A, 0x01, 0x02, 0xC6, 0xF0,
    };
    static const uint8_t want[] = {
        0x11, 0x10, 0x00, 0x01, 0x00, 0x02, 0x12, 0x98,
    };
    modbus_rtu_slave_t s;
    uint8_t resp[32];

    modbus_rtu_slave_init(&s, 0x11, NULL);
    s.write_holding = accept_any_write;

    int resp_len = modbus_rtu_slave_process(&s, req, sizeof(req), resp, sizeof(resp));
    ASSERT_EQ(resp_len, (int)sizeof(want));
    ASSERT_TRUE(memcmp(resp, want, sizeof(want)) == 0);
}

TEST(test_slave_write_multiple_response_follows_the_request)
{
    static const uint8_t req[] = {
        0x01, 0x10, 0x00, 0x0A, 0x00, 0x03, 0x06,
        0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x2F, 0x66,
    };
    static const uint8_t want[] = {
        0x01, 0x10, 0x00, 0x0A, 0x00, 0x03, 0xA0, 0x0A,
    };
    modbus_rtu_slave_t s;
    uint8_t resp[32];

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = accept_any_write;

    int resp_len = modbus_rtu_slave_process(&s, req, sizeof(req), resp, sizeof(resp));
    ASSERT_EQ(resp_len, (int)sizeof(want));
    ASSERT_TRUE(memcmp(resp, want, sizeof(want)) == 0);
}

static int read_from_address(void *ctx, uint16_t addr, uint16_t *value)
{
    (void)ctx;
    *value = (uint16_t)(addr ^ 0xA5A5u);
    return 0;
}

TEST(test_slave_reads_exactly_125_registers)
{
    modbus_rtu_slave_t s;
    uint8_t req[8];
    uint8_t resp[MODBUS_RTU_MAX_FRAME];

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = read_from_address;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req), 0x01,
                                              MODBUS_FC_READ_HOLDING_REGS,
                                              0x0100, 125);
    ASSERT_EQ(req_len, 8);

    /* Slave, function, byte count, 250 data bytes and the CRC. */
    int resp_len = modbus_rtu_slave_process(&s, req, 8, resp, sizeof(resp));
    ASSERT_EQ(resp_len, 255);

    modbus_rtu_frame_t frame;
    ASSERT_EQ(modbus_rtu_parse(resp, (uint16_t)resp_len, &frame), 0);
    ASSERT_TRUE(!modbus_rtu_is_exception(&frame));
    ASSERT_EQ(frame.data[0], 250);

    uint16_t regs[125];
    ASSERT_EQ(modbus_rtu_master_get_regs(&frame, regs, 125), 125);
    for (uint16_t i = 0; i < 125u; i++) {
        ASSERT_EQ(regs[i], (uint16_t)((0x0100u + i) ^ 0xA5A5u));
    }
}

TEST(test_slave_refuses_126_registers)
{
    static const uint8_t data[] = { 0x00, 0x00, 0x00, 0x7E };
    modbus_rtu_slave_t s;
    uint8_t req[8];
    uint8_t resp[MODBUS_RTU_MAX_FRAME];

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = read_from_address;

    /* The master refuses to build this, so it goes in by hand. */
    int req_len = modbus_rtu_build(req, sizeof(req), 0x01,
                                   MODBUS_FC_READ_HOLDING_REGS, data, sizeof(data));
    int resp_len = modbus_rtu_slave_process(&s, req, (uint16_t)req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    ASSERT_EQ(modbus_rtu_parse(resp, (uint16_t)resp_len, &frame), 0);
    ASSERT_TRUE(modbus_rtu_is_exception(&frame));
    ASSERT_EQ(modbus_rtu_exception_code(&frame), MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(test_master_get_regs_one_byte_short)
{
    /* The byte count says 4 and three data bytes follow it. */
    static const uint8_t short_data[] = { 0x04, 0x00, 0x64, 0x00 };
    uint8_t buf[32];
    uint16_t regs[4];
    modbus_rtu_frame_t frame;

    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, short_data, 4);
    ASSERT_EQ(modbus_rtu_parse(buf, (uint16_t)len, &frame), 0);
    ASSERT_EQ(modbus_rtu_master_get_regs(&frame, regs, 4), MODBUS_RTU_ERR_OVERFLOW);
}

TEST_MAIN("modbus_rtu")
    RUN(test_build_and_check_crc);
    RUN(test_build_with_data);
    RUN(test_build_buffer_too_small);
    RUN(test_parse_valid);
    RUN(test_parse_minimal);
    RUN(test_parse_bad_crc);
    RUN(test_parse_too_short);
    RUN(test_parse_null_safety);
    RUN(test_exception_helpers);
    RUN(test_master_read_holding);
    RUN(test_master_read_input);
    RUN(test_master_read_invalid);
    RUN(test_master_write_single);
    RUN(test_master_write_multiple);
    RUN(test_master_write_multiple_invalid);
    RUN(test_master_get_regs);
    RUN(test_master_get_regs_truncate);
    RUN(test_master_get_regs_invalid);
    RUN(test_slave_init);
    RUN(test_slave_read_holding);
    RUN(test_slave_read_input);
    RUN(test_slave_write_single);
    RUN(test_slave_write_multiple);
    RUN(test_slave_address_mismatch);
    RUN(test_slave_broadcast_write);
    RUN(test_slave_broadcast_read_ignored);
    RUN(test_slave_unknown_function);
    RUN(test_slave_no_callback);
    RUN(test_slave_callback_failure);
    RUN(test_slave_write_callback_failure);
    RUN(test_slave_null_safety);
    RUN(test_roundtrip_read);
    RUN(test_roundtrip_write_single);
    RUN(test_roundtrip_write_multiple);
    RUN(test_build_max_data);
    RUN(test_build_exceeds_max_frame);
    RUN(test_slave_reinit_address);
    RUN(test_crc_corruption_middle_byte);
    RUN(test_build_null_data_nonzero_len);
    RUN(test_slave_resp_buffer_too_small);
    RUN(test_slave_write_multiple_mismatched_count);
    RUN(test_slave_read_max_regs);
    RUN(test_build_data_len_overflow);
    RUN(test_parse_exact_min_frame);
    RUN(test_slave_address_wrap);
    RUN(test_slave_write_multiple_whole_response);
    RUN(test_slave_write_multiple_response_follows_the_request);
    RUN(test_slave_reads_exactly_125_registers);
    RUN(test_slave_refuses_126_registers);
    RUN(test_master_get_regs_one_byte_short);
TEST_MAIN_END()

