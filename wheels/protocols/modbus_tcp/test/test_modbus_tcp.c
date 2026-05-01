#include "test_harness.h"
#include "modbus_tcp.h"
#include <string.h>

/* ============================================================ */
/*                 BUILD READ_REGS TESTS                        */
/* ============================================================ */

TEST(test_build_read_regs)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_read_regs(buf, sizeof(buf),
                                         0x0001, 0x01,
                                         0x006B, 3);
    ASSERT_EQ(len, 12);

    /* MBAP header */
    ASSERT_EQ(buf[0], 0x00); ASSERT_EQ(buf[1], 0x01);  /* transaction_id */
    ASSERT_EQ(buf[2], 0x00); ASSERT_EQ(buf[3], 0x00);  /* protocol_id */
    ASSERT_EQ(buf[4], 0x00); ASSERT_EQ(buf[5], 0x06);  /* length = 6 */
    ASSERT_EQ(buf[6], 0x01);                            /* unit_id */

    /* PDU */
    ASSERT_EQ(buf[7], 0x03);                            /* FC 03 */
    ASSERT_EQ(buf[8], 0x00); ASSERT_EQ(buf[9], 0x6B);  /* start_addr */
    ASSERT_EQ(buf[10], 0x00); ASSERT_EQ(buf[11], 0x03); /* count */
}

/* ============================================================ */
/*                 BUILD WRITE_REG TESTS                        */
/* ============================================================ */

TEST(test_build_write_reg)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_write_reg(buf, sizeof(buf),
                                         0x0002, 0x01,
                                         0x0001, 0x0003);
    ASSERT_EQ(len, 12);

    /* MBAP header */
    ASSERT_EQ(buf[0], 0x00); ASSERT_EQ(buf[1], 0x02);  /* transaction_id */
    ASSERT_EQ(buf[2], 0x00); ASSERT_EQ(buf[3], 0x00);  /* protocol_id */
    ASSERT_EQ(buf[4], 0x00); ASSERT_EQ(buf[5], 0x06);  /* length = 6 */
    ASSERT_EQ(buf[6], 0x01);                            /* unit_id */

    /* PDU */
    ASSERT_EQ(buf[7], 0x06);                            /* FC 06 */
    ASSERT_EQ(buf[8], 0x00); ASSERT_EQ(buf[9], 0x01);  /* addr */
    ASSERT_EQ(buf[10], 0x00); ASSERT_EQ(buf[11], 0x03); /* value */
}

/* ============================================================ */
/*                 BUILD WRITE_REGS TESTS                       */
/* ============================================================ */

TEST(test_build_write_regs)
{
    uint8_t buf[64];
    uint16_t values[] = {0x000A, 0x0102};
    int len = modbus_tcp_build_write_regs(buf, sizeof(buf),
                                          0x0003, 0x01,
                                          0x0001, values, 2);
    ASSERT_EQ(len, 17);  /* MBAP(7) + FC(1) + start(2) + count(2) + byte_count(1) + data(4) */

    /* MBAP header */
    ASSERT_EQ(buf[0], 0x00); ASSERT_EQ(buf[1], 0x03);  /* transaction_id */
    ASSERT_EQ(buf[2], 0x00); ASSERT_EQ(buf[3], 0x00);  /* protocol_id */
    ASSERT_EQ(buf[4], 0x00); ASSERT_EQ(buf[5], 11);    /* length = 11 (unit + PDU) */
    ASSERT_EQ(buf[6], 0x01);                            /* unit_id */

    /* PDU */
    ASSERT_EQ(buf[7], 0x10);                            /* FC 16 */
    ASSERT_EQ(buf[8], 0x00); ASSERT_EQ(buf[9], 0x01);  /* start_addr */
    ASSERT_EQ(buf[10], 0x00); ASSERT_EQ(buf[11], 0x02); /* count */
    ASSERT_EQ(buf[12], 0x04);                           /* byte_count */
    ASSERT_EQ(buf[13], 0x00); ASSERT_EQ(buf[14], 0x0A); /* values[0] */
    ASSERT_EQ(buf[15], 0x01); ASSERT_EQ(buf[16], 0x02); /* values[1] */
}

/* ============================================================ */
/*                 PARSE TESTS                                  */
/* ============================================================ */

TEST(test_parse_read_response)
{
    /* FC 03 response: unit=1, byte_count=4, regs=[0x0064, 0x00C8] */
    uint8_t raw[] = {
        0x00, 0x01,   /* transaction_id = 1 */
        0x00, 0x00,   /* protocol_id = 0 */
        0x00, 0x07,   /* length = 7 (unit + FC + byte_count + 4 data bytes) */
        0x01,         /* unit_id */
        0x03,         /* FC 03 */
        0x04,         /* byte_count = 4 */
        0x00, 0x64,   /* reg[0] = 100 */
        0x00, 0xC8,   /* reg[1] = 200 */
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.header.transaction_id, 0x0001);
    ASSERT_EQ(frame.header.protocol_id, 0);
    ASSERT_EQ(frame.header.length, 7);
    ASSERT_EQ(frame.header.unit_id, 0x01);
    ASSERT_EQ(frame.function_code, 0x03);
    ASSERT_EQ(frame.data_len, 5);   /* byte_count(1) + reg_data(4) */
    ASSERT_TRUE(frame.data != NULL);
    ASSERT_EQ(frame.data[0], 0x04); /* byte_count */
}

TEST(test_parse_write_response)
{
    /* FC 06 echo response: addr=0x0001, value=0x0003 */
    uint8_t raw[] = {
        0x00, 0x02,  /* transaction_id */
        0x00, 0x00,  /* protocol_id */
        0x00, 0x06,  /* length = 6 */
        0x01,        /* unit_id */
        0x06,        /* FC 06 */
        0x00, 0x01,  /* addr */
        0x00, 0x03,  /* value */
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.function_code, 0x06);
    ASSERT_EQ(frame.data_len, 4);
}

TEST(test_parse_exception_response)
{
    /* Exception: FC 0x83, exception code 0x02 (ILLEGAL_DATA_ADDR) */
    uint8_t raw[] = {
        0x00, 0x05,  /* transaction_id */
        0x00, 0x00,  /* protocol_id */
        0x00, 0x03,  /* length = 3 (unit + FC|0x80 + exception_code) */
        0x01,        /* unit_id */
        0x83,        /* FC 03 | 0x80 */
        0x02,        /* exception code */
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);
    ASSERT_TRUE(modbus_tcp_is_exception(&frame));
    ASSERT_EQ(modbus_tcp_exception_code(&frame), 0x02);
}

TEST(test_parse_short_buffer)
{
    uint8_t raw[] = {0x00, 0x01, 0x00, 0x00, 0x00};
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, 5, &frame), MODBUS_TCP_ERR_SHORT);
    /* 7 bytes exactly (MBAP only, no FC) */
    uint8_t raw2[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x01};
    ASSERT_EQ(modbus_tcp_parse(raw2, 7, &frame), MODBUS_TCP_ERR_SHORT);
}

TEST(test_parse_wrong_protocol)
{
    /* protocol_id = 1 (not 0) */
    uint8_t raw[] = {
        0x00, 0x01,
        0x00, 0x01,  /* protocol_id = 1 --- WRONG */
        0x00, 0x02,
        0x01,
        0x03,
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_ERR_PROTOCOL);
}

/* ============================================================ */
/*                 GET_REGS TESTS                               */
/* ============================================================ */

TEST(test_get_regs)
{
    /* Build a read response and parse it, then extract registers */
    uint8_t raw[] = {
        0x00, 0x01,  /* transaction_id */
        0x00, 0x00,  /* protocol_id */
        0x00, 0x07,  /* length = 7 */
        0x01,        /* unit_id */
        0x03,        /* FC 03 */
        0x04,        /* byte_count = 4 */
        0x00, 0x64,  /* reg[0] = 100 */
        0x00, 0xC8,  /* reg[1] = 200 */
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);

    uint16_t values[4];
    uint16_t actual;
    ASSERT_EQ(modbus_tcp_get_regs(&frame, values, 4, &actual), MODBUS_TCP_OK);
    ASSERT_EQ(actual, 2);
    ASSERT_EQ(values[0], 0x0064);
    ASSERT_EQ(values[1], 0x00C8);
}

/* ============================================================ */
/*                 TRANSACTION ID PRESERVATION                  */
/* ============================================================ */

TEST(test_transaction_id_preservation)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_read_regs(buf, sizeof(buf),
                                         0x1234, 0x01, 0, 1);
    ASSERT_TRUE(len > 0);

    /* Simulate a response with same transaction_id */
    uint8_t resp[] = {
        0x12, 0x34,  /* transaction_id = 0x1234 */
        0x00, 0x00,  /* protocol_id */
        0x00, 0x05,  /* length = 5 */
        0x01,        /* unit_id */
        0x03,        /* FC 03 */
        0x02,        /* byte_count = 2 */
        0x00, 0x42,  /* reg = 0x0042 */
    };

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(resp, sizeof(resp), &frame), MODBUS_TCP_OK);
    ASSERT_EQ_HEX(frame.header.transaction_id, 0x1234);

    /* Verify the request also has the right transaction_id */
    ASSERT_EQ(buf[0], 0x12);
    ASSERT_EQ(buf[1], 0x34);
}

/* ============================================================ */
/*                 UNIT ID ROUND-TRIP                           */
/* ============================================================ */

TEST(test_unit_id_roundtrip)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_read_regs(buf, sizeof(buf),
                                         0x0001, 0xFE, 0, 1);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[6], 0xFE);  /* unit_id in MBAP */

    /* Parse it back */
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(buf, (uint16_t)len, &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.header.unit_id, 0xFE);
}

/* ============================================================ */
/*                 BUFFER TOO SMALL                             */
/* ============================================================ */

TEST(test_build_buffer_too_small)
{
    uint8_t buf[8];

    /* read_regs needs 12 bytes */
    ASSERT_EQ(modbus_tcp_build_read_regs(buf, 8, 1, 1, 0, 1),
              MODBUS_TCP_ERR_OVERFLOW);

    /* write_reg needs 12 bytes */
    ASSERT_EQ(modbus_tcp_build_write_reg(buf, 8, 1, 1, 0, 0),
              MODBUS_TCP_ERR_OVERFLOW);

    /* write_regs needs 15+ bytes */
    uint16_t v = 1;
    ASSERT_EQ(modbus_tcp_build_write_regs(buf, 8, 1, 1, 0, &v, 1),
              MODBUS_TCP_ERR_OVERFLOW);
}

/* ============================================================ */
/*                 NULL PARAMS                                  */
/* ============================================================ */

TEST(test_null_params)
{
    modbus_tcp_frame_t frame;
    uint8_t buf[32];
    uint16_t v = 1;

    ASSERT_EQ(modbus_tcp_build_read_regs(NULL, 32, 1, 1, 0, 1),
              MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_build_write_reg(NULL, 32, 1, 1, 0, 0),
              MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_build_write_regs(NULL, 32, 1, 1, 0, &v, 1),
              MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_build_write_regs(buf, 32, 1, 1, 0, NULL, 1),
              MODBUS_TCP_ERR_PARAMS);

    ASSERT_EQ(modbus_tcp_parse(NULL, 12, &frame), MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_parse(buf, 12, NULL), MODBUS_TCP_ERR_PARAMS);

    uint16_t vals[2];
    uint16_t actual;
    ASSERT_EQ(modbus_tcp_get_regs(NULL, vals, 2, &actual), MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_get_regs(&frame, NULL, 2, &actual), MODBUS_TCP_ERR_PARAMS);
    ASSERT_EQ(modbus_tcp_get_regs(&frame, vals, 0, &actual), MODBUS_TCP_ERR_PARAMS);
}

/* ============================================================ */
/*                 MULTIPLE REGISTERS (5+)                      */
/* ============================================================ */

TEST(test_multiple_regs_five)
{
    /* Build a write_regs with 5 values */
    uint8_t buf[64];
    uint16_t values[] = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005};
    int len = modbus_tcp_build_write_regs(buf, sizeof(buf),
                                          0x0010, 0x01,
                                          0x0000, values, 5);
    /* MBAP(7) + FC(1) + start(2) + count(2) + byte_count(1) + data(10) = 23 */
    ASSERT_EQ(len, 23);

    /* Parse it back */
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(buf, (uint16_t)len, &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.function_code, 0x10);
    ASSERT_EQ(frame.header.transaction_id, 0x0010);

    /* Verify byte count */
    ASSERT_EQ(frame.data[4], 10);  /* 5 regs * 2 bytes */

    /* Build a read response with 5 regs and extract */
    uint8_t resp[] = {
        0x00, 0x10,  /* transaction_id */
        0x00, 0x00,  /* protocol_id */
        0x00, 0x0D,  /* length = 13 (unit + FC + byte_count + 10) */
        0x01,        /* unit_id */
        0x03,        /* FC 03 */
        0x0A,        /* byte_count = 10 */
        0x00, 0x01,  0x00, 0x02,  0x00, 0x03,  0x00, 0x04,  0x00, 0x05,
    };

    modbus_tcp_frame_t resp_frame;
    ASSERT_EQ(modbus_tcp_parse(resp, sizeof(resp), &resp_frame), MODBUS_TCP_OK);

    uint16_t out[8];
    uint16_t actual;
    ASSERT_EQ(modbus_tcp_get_regs(&resp_frame, out, 8, &actual), MODBUS_TCP_OK);
    ASSERT_EQ(actual, 5);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[1], 2);
    ASSERT_EQ(out[2], 3);
    ASSERT_EQ(out[3], 4);
    ASSERT_EQ(out[4], 5);
}

/* ============================================================ */
/*                 BUILD-PARSE ROUND-TRIP                       */
/* ============================================================ */

TEST(test_roundtrip_read_regs)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_read_regs(buf, sizeof(buf),
                                         0x0007, 0x03, 0x0100, 10);
    ASSERT_TRUE(len > 0);

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(buf, (uint16_t)len, &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.header.transaction_id, 0x0007);
    ASSERT_EQ(frame.header.unit_id, 0x03);
    ASSERT_EQ(frame.function_code, 0x03);
    /* PDU data: start(2) + count(2) = 4 bytes */
    ASSERT_EQ(frame.data_len, 4);
}

TEST(test_roundtrip_write_reg)
{
    uint8_t buf[32];
    int len = modbus_tcp_build_write_reg(buf, sizeof(buf),
                                         0x0008, 0x02, 0x000A, 0xBEEF);
    ASSERT_TRUE(len > 0);

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(buf, (uint16_t)len, &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.header.transaction_id, 0x0008);
    ASSERT_EQ(frame.function_code, 0x06);
    ASSERT_EQ(frame.data_len, 4);
}

TEST(test_roundtrip_write_regs)
{
    uint8_t buf[64];
    uint16_t values[] = {0x1111, 0x2222, 0x3333};
    int len = modbus_tcp_build_write_regs(buf, sizeof(buf),
                                          0x0009, 0x04,
                                          0x0000, values, 3);
    ASSERT_TRUE(len > 0);

    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(buf, (uint16_t)len, &frame), MODBUS_TCP_OK);
    ASSERT_EQ(frame.function_code, 0x10);
    ASSERT_EQ(frame.header.unit_id, 0x04);
}

/* ============================================================ */
/*                 EDGE / ERROR CASES                           */
/* ============================================================ */

TEST(test_get_regs_from_exception)
{
    uint8_t raw[] = {
        0x00, 0x01,  0x00, 0x00,  0x00, 0x03,
        0x01,  0x83,  0x02,
    };
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);

    uint16_t vals[2];
    uint16_t actual;
    ASSERT_EQ(modbus_tcp_get_regs(&frame, vals, 2, &actual),
              MODBUS_TCP_ERR_EXCEPTION);
}

TEST(test_get_regs_actual_count_null)
{
    uint8_t raw[] = {
        0x00, 0x01,  0x00, 0x00,  0x00, 0x05,
        0x01,  0x03,  0x02,  0x00, 0x42,
    };
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_OK);

    uint16_t vals[2];
    /* actual_count = NULL is allowed */
    ASSERT_EQ(modbus_tcp_get_regs(&frame, vals, 2, NULL), MODBUS_TCP_OK);
    ASSERT_EQ(vals[0], 0x0042);
}

TEST(test_build_read_regs_zero_count)
{
    uint8_t buf[32];
    ASSERT_EQ(modbus_tcp_build_read_regs(buf, sizeof(buf), 1, 1, 0, 0),
              MODBUS_TCP_ERR_PARAMS);
}

TEST(test_build_write_regs_zero_count)
{
    uint8_t buf[32];
    uint16_t v = 1;
    ASSERT_EQ(modbus_tcp_build_write_regs(buf, sizeof(buf), 1, 1, 0, &v, 0),
              MODBUS_TCP_ERR_PARAMS);
}

TEST(test_parse_length_exceeds_buffer)
{
    /* MBAP says length=10 but buffer only has 10 bytes total */
    uint8_t raw[] = {
        0x00, 0x01,
        0x00, 0x00,
        0x00, 0x0A,  /* length = 10 -- needs 16 bytes total */
        0x01,
        0x03,
        0x00, 0x00,  /* only 10 bytes provided */
    };
    modbus_tcp_frame_t frame;
    ASSERT_EQ(modbus_tcp_parse(raw, sizeof(raw), &frame), MODBUS_TCP_ERR_SHORT);
}

TEST(test_exception_helpers_null)
{
    ASSERT_FALSE(modbus_tcp_is_exception(NULL));
    ASSERT_EQ(modbus_tcp_exception_code(NULL), 0);
}

/* ============================================================ */

TEST_MAIN("modbus_tcp")
    RUN(test_build_read_regs);
    RUN(test_build_write_reg);
    RUN(test_build_write_regs);
    RUN(test_parse_read_response);
    RUN(test_parse_write_response);
    RUN(test_parse_exception_response);
    RUN(test_parse_short_buffer);
    RUN(test_parse_wrong_protocol);
    RUN(test_get_regs);
    RUN(test_transaction_id_preservation);
    RUN(test_unit_id_roundtrip);
    RUN(test_build_buffer_too_small);
    RUN(test_null_params);
    RUN(test_multiple_regs_five);
    RUN(test_roundtrip_read_regs);
    RUN(test_roundtrip_write_reg);
    RUN(test_roundtrip_write_regs);
    RUN(test_get_regs_from_exception);
    RUN(test_get_regs_actual_count_null);
    RUN(test_build_read_regs_zero_count);
    RUN(test_build_write_regs_zero_count);
    RUN(test_parse_length_exceeds_buffer);
    RUN(test_exception_helpers_null);
TEST_MAIN_END()
