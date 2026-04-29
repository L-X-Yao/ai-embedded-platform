#include <stdio.h>
#include <string.h>
#include "../modbus_rtu.h"

/* ---------- test harness ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

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

static void test_build_and_check_crc(void)
{
    uint8_t buf[32];

    printf("test_build_and_check_crc\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT(len == 4, "minimal frame is 4 bytes");
    ASSERT(modbus_rtu_check_crc(buf, len), "CRC valid after build");

    buf[len - 1] ^= 0xFF;
    ASSERT(!modbus_rtu_check_crc(buf, len), "CRC invalid after corruption");
}

static void test_build_with_data(void)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};

    printf("test_build_with_data\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT(len == 8, "frame = 2 + 4 + 2");
    ASSERT(buf[0] == 0x01, "address");
    ASSERT(buf[1] == 0x03, "function");
    ASSERT(buf[2] == 0x00 && buf[3] == 0x6B, "data[0:1]");
    ASSERT(modbus_rtu_check_crc(buf, len), "CRC valid");
}

static void test_build_buffer_too_small(void)
{
    uint8_t buf[4];

    printf("test_build_buffer_too_small\n");
    ASSERT(modbus_rtu_build(buf, 3, 1, 3, NULL, 0) == -1, "buf too small");
    ASSERT(modbus_rtu_build(NULL, 32, 1, 3, NULL, 0) == -1, "null buf");
}

static void test_parse_valid(void)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};
    modbus_rtu_frame_t frame;

    printf("test_parse_valid\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "parse ok");
    ASSERT(frame.addr == 0x01, "addr");
    ASSERT(frame.func == 0x03, "func");
    ASSERT(frame.data_len == 4, "data_len");
    ASSERT(frame.data[0] == 0x00 && frame.data[1] == 0x6B, "data content");
}

static void test_parse_minimal(void)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;

    printf("test_parse_minimal\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "parse minimal ok");
    ASSERT(frame.data == NULL, "no data pointer");
    ASSERT(frame.data_len == 0, "data_len 0");
}

static void test_parse_bad_crc(void)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;

    printf("test_parse_bad_crc\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    buf[len - 1] ^= 0xFF;
    ASSERT(modbus_rtu_parse(buf, len, &frame) == -1, "bad CRC rejected");
}

static void test_parse_too_short(void)
{
    uint8_t buf[] = {0x01, 0x03, 0x00};
    modbus_rtu_frame_t frame;

    printf("test_parse_too_short\n");
    ASSERT(modbus_rtu_parse(buf, 3, &frame) == -1, "too short");
    ASSERT(modbus_rtu_parse(buf, 0, &frame) == -1, "zero length");
}

static void test_parse_null_safety(void)
{
    uint8_t buf[8];
    modbus_rtu_frame_t frame;

    printf("test_parse_null_safety\n");
    ASSERT(modbus_rtu_parse(NULL, 8, &frame) == -1, "null buf");
    ASSERT(modbus_rtu_parse(buf, 8, NULL) == -1, "null frame");
    ASSERT(!modbus_rtu_check_crc(NULL, 8), "check_crc null");
    ASSERT(!modbus_rtu_check_crc(buf, 2), "check_crc too short");
}

static void test_exception_helpers(void)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;

    printf("test_exception_helpers\n");
    uint8_t ex_data = MODBUS_EX_ILLEGAL_FUNCTION;
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x83, &ex_data, 1);
    modbus_rtu_parse(buf, len, &frame);

    ASSERT(modbus_rtu_is_exception(&frame), "is exception");
    ASSERT(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_FUNCTION, "exception code");

    len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    modbus_rtu_parse(buf, len, &frame);
    ASSERT(!modbus_rtu_is_exception(&frame), "not exception");

    ASSERT(!modbus_rtu_is_exception(NULL), "null is_exception");
    ASSERT(modbus_rtu_exception_code(NULL) == 0, "null exception_code");
}

/* ============================================================ */
/*                    MASTER TESTS                              */
/* ============================================================ */

static void test_master_read_holding(void)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;

    printf("test_master_read_holding\n");
    int len = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                          0x01, MODBUS_FC_READ_HOLDING_REGS,
                                          0x006B, 3);
    ASSERT(len == 8, "8 bytes");
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "valid frame");
    ASSERT(frame.addr == 0x01, "slave addr");
    ASSERT(frame.func == 0x03, "FC 03");
    ASSERT(frame.data[0] == 0x00 && frame.data[1] == 0x6B, "start reg");
    ASSERT(frame.data[2] == 0x00 && frame.data[3] == 0x03, "count");
}

static void test_master_read_input(void)
{
    uint8_t buf[32];

    printf("test_master_read_input\n");
    int len = modbus_rtu_master_read_regs(buf, sizeof(buf),
                                          0x02, MODBUS_FC_READ_INPUT_REGS,
                                          0x0000, 1);
    ASSERT(len == 8, "8 bytes");
    ASSERT(buf[1] == 0x04, "FC 04");
}

static void test_master_read_invalid(void)
{
    uint8_t buf[32];

    printf("test_master_read_invalid\n");
    ASSERT(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x01, 0, 1) == -1,
           "invalid FC");
    ASSERT(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x03, 0, 0) == -1,
           "count 0");
    ASSERT(modbus_rtu_master_read_regs(buf, sizeof(buf), 1, 0x03, 0, 126) == -1,
           "count > 125");
}

static void test_master_write_single(void)
{
    uint8_t buf[32];
    modbus_rtu_frame_t frame;

    printf("test_master_write_single\n");
    int len = modbus_rtu_master_write_reg(buf, sizeof(buf),
                                          0x01, 0x0001, 0x0003);
    ASSERT(len == 8, "8 bytes");
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "valid");
    ASSERT(frame.func == 0x06, "FC 06");
    ASSERT(frame.data[0] == 0x00 && frame.data[1] == 0x01, "reg addr");
    ASSERT(frame.data[2] == 0x00 && frame.data[3] == 0x03, "value");
}

static void test_master_write_multiple(void)
{
    uint8_t buf[64];
    modbus_rtu_frame_t frame;
    uint16_t values[] = {0x000A, 0x0102};

    printf("test_master_write_multiple\n");
    int len = modbus_rtu_master_write_regs(buf, sizeof(buf),
                                           0x01, 0x0001, 2, values);
    ASSERT(len > 0, "built ok");
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "valid");
    ASSERT(frame.func == 0x10, "FC 10");
    ASSERT(frame.data[4] == 4, "byte count = 4");
    ASSERT(frame.data[5] == 0x00 && frame.data[6] == 0x0A, "value 0");
    ASSERT(frame.data[7] == 0x01 && frame.data[8] == 0x02, "value 1");
}

static void test_master_write_multiple_invalid(void)
{
    uint8_t buf[64];
    uint16_t v = 1;

    printf("test_master_write_multiple_invalid\n");
    ASSERT(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0, 0, &v) == -1,
           "count 0");
    ASSERT(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0, 1, NULL) == -1,
           "null values");
    ASSERT(modbus_rtu_master_write_regs(buf, sizeof(buf), 1, 0,
           MODBUS_MAX_WRITE_REGS + 1, &v) == -1,
           "count > MODBUS_MAX_WRITE_REGS");
}

static void test_master_get_regs(void)
{
    printf("test_master_get_regs\n");

    /* Build a fake read response: addr=1, FC=03, byte_count=4, regs=[0x0064, 0x00C8] */
    uint8_t resp_buf[32];
    uint8_t resp_data[] = {0x04, 0x00, 0x64, 0x00, 0xC8};
    int len = modbus_rtu_build(resp_buf, sizeof(resp_buf), 0x01, 0x03,
                               resp_data, 5);

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp_buf, len, &frame);

    uint16_t regs[4];
    int n = modbus_rtu_master_get_regs(&frame, regs, 4);
    ASSERT(n == 2, "2 registers");
    ASSERT(regs[0] == 0x0064, "reg 0 = 100");
    ASSERT(regs[1] == 0x00C8, "reg 1 = 200");
}

static void test_master_get_regs_truncate(void)
{
    printf("test_master_get_regs_truncate\n");

    uint8_t resp_buf[32];
    uint8_t resp_data[] = {0x06, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
    int len = modbus_rtu_build(resp_buf, sizeof(resp_buf), 0x01, 0x03,
                               resp_data, 7);
    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp_buf, len, &frame);

    uint16_t regs[2];
    int n = modbus_rtu_master_get_regs(&frame, regs, 2);
    ASSERT(n == 2, "truncated to max_count");
    ASSERT(regs[0] == 1 && regs[1] == 2, "correct values");
}

static void test_master_get_regs_invalid(void)
{
    printf("test_master_get_regs_invalid\n");
    uint16_t regs[4];

    ASSERT(modbus_rtu_master_get_regs(NULL, regs, 4) == -1, "null frame");

    modbus_rtu_frame_t frame = {0};
    ASSERT(modbus_rtu_master_get_regs(&frame, regs, 4) == -1, "null data");

    /* Odd byte count */
    uint8_t buf[32];
    uint8_t bad_data[] = {0x03, 0x00, 0x01, 0x00};
    int len = modbus_rtu_build(buf, sizeof(buf), 1, 3, bad_data, 4);
    modbus_rtu_parse(buf, len, &frame);
    ASSERT(modbus_rtu_master_get_regs(&frame, regs, 4) == -1, "odd byte count");
}

/* ============================================================ */
/*                    SLAVE TESTS                               */
/* ============================================================ */

static void test_slave_init(void)
{
    modbus_rtu_slave_t s;

    printf("test_slave_init\n");
    modbus_rtu_slave_init(&s, 0x01, NULL);
    ASSERT(s.addr == 0x01, "addr set");
    ASSERT(s.read_holding == NULL, "no callbacks");
    ASSERT(s.ctx == NULL, "ctx null");

    modbus_rtu_slave_init(NULL, 1, NULL);  /* should not crash */
    ASSERT(1, "null init safe");
}

static void test_slave_read_holding(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_read_holding\n");
    mock_reset();
    holding_regs[0] = 0x1234;
    holding_regs[1] = 0x5678;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 2);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "response built");

    modbus_rtu_frame_t frame;
    ASSERT(modbus_rtu_parse(resp, resp_len, &frame) == 0, "response valid");
    ASSERT(frame.func == 0x03, "FC 03");

    uint16_t regs[2];
    int n = modbus_rtu_master_get_regs(&frame, regs, 2);
    ASSERT(n == 2, "2 regs");
    ASSERT(regs[0] == 0x1234, "reg 0");
    ASSERT(regs[1] == 0x5678, "reg 1");
}

static void test_slave_read_input(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_read_input\n");
    mock_reset();
    input_regs[5] = 0xABCD;

    modbus_rtu_slave_init(&s, 0x02, NULL);
    s.read_input = mock_read_input;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x02, MODBUS_FC_READ_INPUT_REGS,
                                               5, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "response built");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    uint16_t val;
    modbus_rtu_master_get_regs(&frame, &val, 1);
    ASSERT(val == 0xABCD, "input reg value");
}

static void test_slave_write_single(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_slave_write_single\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x01, 3, 0x00FF);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "response built");
    ASSERT(holding_regs[3] == 0x00FF, "register written");
    ASSERT(write_count == 1, "one write");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(frame.func == 0x06, "FC 06 echo");
}

static void test_slave_write_multiple(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[64], resp[32];
    uint16_t values[] = {0x000A, 0x000B, 0x000C};

    printf("test_slave_write_multiple\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_regs(req, sizeof(req),
                                                0x01, 10, 3, values);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "response built");
    ASSERT(holding_regs[10] == 0x000A, "reg 10");
    ASSERT(holding_regs[11] == 0x000B, "reg 11");
    ASSERT(holding_regs[12] == 0x000C, "reg 12");
    ASSERT(write_count == 3, "three writes");
}

static void test_slave_address_mismatch(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_slave_address_mismatch\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x02, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len == 0, "not for us");
}

static void test_slave_broadcast_write(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_slave_broadcast_write\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x00, 0, 0x1234);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len == 0, "no response to broadcast");
    ASSERT(holding_regs[0] == 0x1234, "write executed");
}

static void test_slave_broadcast_read_ignored(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_broadcast_read_ignored\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x00, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len == 0, "broadcast read: no response");
}

static void test_slave_unknown_function(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_slave_unknown_function\n");
    modbus_rtu_slave_init(&s, 0x01, NULL);

    int req_len = modbus_rtu_build(req, sizeof(req), 0x01, 0x01, NULL, 0);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "exception response");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(modbus_rtu_is_exception(&frame), "is exception");
    ASSERT(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_FUNCTION,
           "illegal function");
}

static void test_slave_no_callback(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_no_callback\n");
    modbus_rtu_slave_init(&s, 0x01, NULL);
    /* read_holding not set */

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(modbus_rtu_is_exception(&frame), "exception for missing cb");
}

static void test_slave_callback_failure(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_callback_failure\n");
    mock_reset();
    fail_next_read = 1;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(modbus_rtu_is_exception(&frame), "exception on cb failure");
    ASSERT(modbus_rtu_exception_code(&frame) == MODBUS_EX_SLAVE_FAILURE,
           "slave failure code");
}

static void test_slave_write_callback_failure(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_slave_write_callback_failure\n");
    mock_reset();
    fail_next_write = 1;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req), 0x01, 0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(modbus_rtu_is_exception(&frame), "exception on write failure");
}

static void test_slave_null_safety(void)
{
    uint8_t req[32], resp[32];

    printf("test_slave_null_safety\n");
    ASSERT(modbus_rtu_slave_process(NULL, req, 8, resp, 32) == -1, "null slave");

    modbus_rtu_slave_t s;
    modbus_rtu_slave_init(&s, 1, NULL);
    ASSERT(modbus_rtu_slave_process(&s, NULL, 8, resp, 32) == -1, "null req");
    ASSERT(modbus_rtu_slave_process(&s, req, 8, NULL, 32) == -1, "null resp");
}

/* ============================================================ */
/*                    ROUND-TRIP INTEGRATION                    */
/* ============================================================ */

static void test_roundtrip_read(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_roundtrip_read\n");
    mock_reset();
    holding_regs[0] = 100;
    holding_regs[1] = 200;
    holding_regs[2] = 300;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 3);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);

    uint16_t regs[3];
    int n = modbus_rtu_master_get_regs(&frame, regs, 3);
    ASSERT(n == 3, "3 regs round-trip");
    ASSERT(regs[0] == 100, "reg 0 round-trip");
    ASSERT(regs[1] == 200, "reg 1 round-trip");
    ASSERT(regs[2] == 300, "reg 2 round-trip");
}

static void test_roundtrip_write_single(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[32];

    printf("test_roundtrip_write_single\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_reg(req, sizeof(req),
                                               0x01, 5, 0xBEEF);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(holding_regs[5] == 0xBEEF, "written via round-trip");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(!modbus_rtu_is_exception(&frame), "no exception");
    ASSERT(frame.func == 0x06, "echo FC 06");
}

static void test_roundtrip_write_multiple(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[64], resp[32];
    uint16_t values[] = {0x1111, 0x2222};

    printf("test_roundtrip_write_multiple\n");
    mock_reset();
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.write_holding = mock_write_holding;

    int req_len = modbus_rtu_master_write_regs(req, sizeof(req),
                                                0x01, 0, 2, values);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(holding_regs[0] == 0x1111, "multi-write reg 0");
    ASSERT(holding_regs[1] == 0x2222, "multi-write reg 1");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(!modbus_rtu_is_exception(&frame), "no exception");
    ASSERT(frame.func == 0x10, "FC 10");
}

/* ============================================================ */
/*                    BOUNDARY TESTS                           */
/* ============================================================ */

static void test_build_max_data(void)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME];
    uint8_t data[MODBUS_RTU_MAX_FRAME - 4];

    printf("test_build_max_data\n");
    memset(data, 0xAA, sizeof(data));
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03,
                               data, MODBUS_RTU_MAX_FRAME - 4);
    ASSERT(len == MODBUS_RTU_MAX_FRAME, "max frame built");
    ASSERT(modbus_rtu_check_crc(buf, len), "CRC valid at max size");

    modbus_rtu_frame_t frame;
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "max frame parseable");
    ASSERT(frame.data_len == MODBUS_RTU_MAX_FRAME - 4, "max data_len");
}

static void test_build_exceeds_max_frame(void)
{
    uint8_t buf[MODBUS_RTU_MAX_FRAME + 4];
    uint8_t data[MODBUS_RTU_MAX_FRAME - 3];

    printf("test_build_exceeds_max_frame\n");
    memset(data, 0xBB, sizeof(data));
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03,
                               data, MODBUS_RTU_MAX_FRAME - 3);
    ASSERT(len == -1, "oversized frame rejected");
}

static void test_slave_reinit_address(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[256];

    printf("test_slave_reinit_address\n");
    mock_reset();
    holding_regs[0] = 0xAAAA;

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    ASSERT(modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp)) > 0,
           "responds to addr 0x01");

    modbus_rtu_slave_init(&s, 0x05, NULL);
    s.read_holding = mock_read_holding;

    ASSERT(modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp)) == 0,
           "ignores old addr after re-init");

    req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                           0x05, MODBUS_FC_READ_HOLDING_REGS,
                                           0, 1);
    ASSERT(modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp)) > 0,
           "responds to new addr 0x05");
}

static void test_crc_corruption_middle_byte(void)
{
    uint8_t buf[32];
    uint8_t data[] = {0x00, 0x6B, 0x00, 0x03};
    modbus_rtu_frame_t frame;

    printf("test_crc_corruption_middle_byte\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, data, 4);
    ASSERT(len == 8, "8 bytes");
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "valid before corruption");

    buf[3] ^= 0xFF;
    ASSERT(modbus_rtu_parse(buf, len, &frame) == -1, "middle byte corruption detected");

    buf[3] ^= 0xFF;
    buf[0] ^= 0x01;
    ASSERT(modbus_rtu_parse(buf, len, &frame) == -1, "first byte corruption detected");
}

static void test_build_null_data_nonzero_len(void)
{
    uint8_t buf[32];

    printf("test_build_null_data_nonzero_len\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 4);
    ASSERT(len == 8, "frame built despite null data");
    ASSERT(modbus_rtu_check_crc(buf, len), "CRC valid");
    ASSERT(buf[0] == 0x01, "addr preserved");
    ASSERT(buf[1] == 0x03, "func preserved");
}

static void test_slave_resp_buffer_too_small(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[4];

    printf("test_slave_resp_buffer_too_small\n");
    mock_reset();
    holding_regs[0] = 0x1234;
    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, 1);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len == -1, "resp buffer too small returns -1");
}

static void test_slave_write_multiple_mismatched_count(void)
{
    modbus_rtu_slave_t s;
    uint8_t raw[32], resp[64];

    printf("test_slave_write_multiple_mismatched_count\n");
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
    int resp_len = modbus_rtu_slave_process(&s, raw, raw_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "exception response");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(modbus_rtu_is_exception(&frame), "mismatched count triggers exception");
    ASSERT(modbus_rtu_exception_code(&frame) == MODBUS_EX_ILLEGAL_DATA_VALUE,
           "illegal data value");
}

static void test_slave_read_max_regs(void)
{
    modbus_rtu_slave_t s;
    uint8_t req[32], resp[512];

    printf("test_slave_read_max_regs\n");
    mock_reset();
    for (int i = 0; i < REG_BANK_SIZE; i++)
        holding_regs[i] = (uint16_t)(i + 1);

    modbus_rtu_slave_init(&s, 0x01, NULL);
    s.read_holding = mock_read_holding;

    int req_len = modbus_rtu_master_read_regs(req, sizeof(req),
                                               0x01, MODBUS_FC_READ_HOLDING_REGS,
                                               0, REG_BANK_SIZE);
    int resp_len = modbus_rtu_slave_process(&s, req, req_len, resp, sizeof(resp));
    ASSERT(resp_len > 0, "response for max regs");

    modbus_rtu_frame_t frame;
    modbus_rtu_parse(resp, resp_len, &frame);
    ASSERT(!modbus_rtu_is_exception(&frame), "no exception");

    uint16_t regs[REG_BANK_SIZE];
    int n = modbus_rtu_master_get_regs(&frame, regs, REG_BANK_SIZE);
    ASSERT(n == REG_BANK_SIZE, "all regs returned");
    ASSERT(regs[0] == 1, "first reg correct");
    ASSERT(regs[REG_BANK_SIZE - 1] == REG_BANK_SIZE, "last reg correct");
}

static void test_parse_exact_min_frame(void)
{
    uint8_t buf[4];
    modbus_rtu_frame_t frame;

    printf("test_parse_exact_min_frame\n");
    int len = modbus_rtu_build(buf, sizeof(buf), 0x01, 0x03, NULL, 0);
    ASSERT(len == 4, "minimum frame is 4 bytes");
    ASSERT(modbus_rtu_parse(buf, len, &frame) == 0, "parseable");
    ASSERT(frame.data == NULL, "no data");
    ASSERT(frame.data_len == 0, "data_len 0");
    ASSERT(frame.addr == 0x01, "addr");
    ASSERT(frame.func == 0x03, "func");
}

/* ============================================================ */

int main(void)
{
    tests_run = tests_fail = 0;

    /* Frame layer */
    test_build_and_check_crc();
    test_build_with_data();
    test_build_buffer_too_small();
    test_parse_valid();
    test_parse_minimal();
    test_parse_bad_crc();
    test_parse_too_short();
    test_parse_null_safety();
    test_exception_helpers();

    /* Master */
    test_master_read_holding();
    test_master_read_input();
    test_master_read_invalid();
    test_master_write_single();
    test_master_write_multiple();
    test_master_write_multiple_invalid();
    test_master_get_regs();
    test_master_get_regs_truncate();
    test_master_get_regs_invalid();

    /* Slave */
    test_slave_init();
    test_slave_read_holding();
    test_slave_read_input();
    test_slave_write_single();
    test_slave_write_multiple();
    test_slave_address_mismatch();
    test_slave_broadcast_write();
    test_slave_broadcast_read_ignored();
    test_slave_unknown_function();
    test_slave_no_callback();
    test_slave_callback_failure();
    test_slave_write_callback_failure();
    test_slave_null_safety();

    /* Integration */
    test_roundtrip_read();
    test_roundtrip_write_single();
    test_roundtrip_write_multiple();

    /* Boundary */
    test_build_max_data();
    test_build_exceeds_max_frame();
    test_slave_reinit_address();
    test_crc_corruption_middle_byte();
    test_build_null_data_nonzero_len();
    test_slave_resp_buffer_too_small();
    test_slave_write_multiple_mismatched_count();
    test_slave_read_max_regs();
    test_parse_exact_min_frame();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
