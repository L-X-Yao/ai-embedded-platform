#include "test_harness.h"
#include "tftp.h"
#include <string.h>

/* ---------- RRQ / WRQ build + parse roundtrip ---------- */

TEST(test_build_parse_rrq)
{
    uint8_t buf[256];
    int n = tftp_build_rrq(buf, sizeof(buf), "firmware.bin", "octet");
    ASSERT_TRUE(n > 0);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_RRQ);
    ASSERT_STR_EQ(pkt.u.rq.filename, "firmware.bin");
    ASSERT_STR_EQ(pkt.u.rq.mode, "octet");
}

TEST(test_build_parse_wrq)
{
    uint8_t buf[256];
    int n = tftp_build_wrq(buf, sizeof(buf), "config.txt", "netascii");
    ASSERT_TRUE(n > 0);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_WRQ);
    ASSERT_STR_EQ(pkt.u.rq.filename, "config.txt");
    ASSERT_STR_EQ(pkt.u.rq.mode, "netascii");
}

TEST(test_rrq_octet_mode)
{
    uint8_t buf[256];
    int n = tftp_build_rrq(buf, sizeof(buf), "test.dat", "octet");
    ASSERT_TRUE(n > 0);

    /* verify wire format: opcode(2) + "test.dat" + NUL + "octet" + NUL */
    ASSERT_EQ(buf[0], 0);
    ASSERT_EQ(buf[1], TFTP_OP_RRQ);
    ASSERT_MEMEQ(buf + 2, "test.dat");
    ASSERT_EQ(buf[10], 0);  /* NUL after filename */
    ASSERT_MEMEQ(buf + 11, "octet");
    ASSERT_EQ(buf[16], 0);  /* NUL after mode */
    ASSERT_EQ(n, 17);
}

/* ---------- DATA build + parse roundtrip ---------- */

TEST(test_build_parse_data_full)
{
    uint8_t buf[600];
    uint8_t payload[TFTP_BLOCK_SIZE];
    memset(payload, 0xAB, sizeof(payload));

    int n = tftp_build_data(buf, sizeof(buf), 1, payload, sizeof(payload));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 4 + TFTP_BLOCK_SIZE);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_DATA);
    ASSERT_EQ(pkt.u.data.block, 1);
    ASSERT_EQ(pkt.u.data.len, (size_t)TFTP_BLOCK_SIZE);
    ASSERT_NOT_NULL(pkt.u.data.data);
    ASSERT_MEMEQ_N(pkt.u.data.data, payload, TFTP_BLOCK_SIZE);
    ASSERT_FALSE(tftp_is_last_data(&pkt));
}

TEST(test_build_parse_data_partial)
{
    uint8_t buf[256];
    uint8_t payload[100];
    memset(payload, 0xCD, sizeof(payload));

    int n = tftp_build_data(buf, sizeof(buf), 5, payload, sizeof(payload));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 4 + 100);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_DATA);
    ASSERT_EQ(pkt.u.data.block, 5);
    ASSERT_EQ(pkt.u.data.len, 100U);
    ASSERT_TRUE(tftp_is_last_data(&pkt));
}

TEST(test_build_parse_data_empty)
{
    uint8_t buf[16];
    int n = tftp_build_data(buf, sizeof(buf), 3, NULL, 0);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 4);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_DATA);
    ASSERT_EQ(pkt.u.data.block, 3);
    ASSERT_EQ(pkt.u.data.len, 0U);
    ASSERT_TRUE(tftp_is_last_data(&pkt));
}

/* ---------- ACK build + parse roundtrip ---------- */

TEST(test_build_parse_ack)
{
    uint8_t buf[16];
    int n = tftp_build_ack(buf, sizeof(buf), 42);
    ASSERT_EQ(n, 4);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_ACK);
    ASSERT_EQ(pkt.u.ack.block, 42);
}

TEST(test_ack_block_zero)
{
    uint8_t buf[16];
    int n = tftp_build_ack(buf, sizeof(buf), 0);
    ASSERT_EQ(n, 4);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.u.ack.block, 0);
}

TEST(test_ack_block_max)
{
    uint8_t buf[16];
    int n = tftp_build_ack(buf, sizeof(buf), 0xFFFF);
    ASSERT_EQ(n, 4);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.u.ack.block, 0xFFFF);
}

/* ---------- ERROR build + parse roundtrip ---------- */

TEST(test_build_parse_error)
{
    uint8_t buf[256];
    int n = tftp_build_error(buf, sizeof(buf), TFTP_ERRC_NOT_FOUND,
                             "File not found");
    ASSERT_TRUE(n > 0);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_ERROR);
    ASSERT_EQ(pkt.u.error.code, TFTP_ERRC_NOT_FOUND);
    ASSERT_STR_EQ(pkt.u.error.msg, "File not found");
}

TEST(test_error_disk_full)
{
    uint8_t buf[256];
    int n = tftp_build_error(buf, sizeof(buf), TFTP_ERRC_DISK_FULL,
                             "Disk full");
    ASSERT_TRUE(n > 0);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.u.error.code, TFTP_ERRC_DISK_FULL);
    ASSERT_STR_EQ(pkt.u.error.msg, "Disk full");
}

/* ---------- last block detection ---------- */

TEST(test_is_last_data_full_block)
{
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = TFTP_OP_DATA;
    pkt.u.data.len = TFTP_BLOCK_SIZE;
    ASSERT_FALSE(tftp_is_last_data(&pkt));
}

TEST(test_is_last_data_partial)
{
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = TFTP_OP_DATA;
    pkt.u.data.len = 200;
    ASSERT_TRUE(tftp_is_last_data(&pkt));
}

TEST(test_is_last_data_zero)
{
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = TFTP_OP_DATA;
    pkt.u.data.len = 0;
    ASSERT_TRUE(tftp_is_last_data(&pkt));
}

TEST(test_is_last_data_non_data_pkt)
{
    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = TFTP_OP_ACK;
    ASSERT_FALSE(tftp_is_last_data(&pkt));
}

TEST(test_is_last_data_null)
{
    ASSERT_FALSE(tftp_is_last_data(NULL));
}

/* ---------- filename truncation ---------- */

TEST(test_large_filename_truncation)
{
    char longname[256];
    memset(longname, 'A', 200);
    longname[200] = '\0';

    uint8_t buf[512];
    int n = tftp_build_rrq(buf, sizeof(buf), longname, "octet");
    ASSERT_TRUE(n > 0);

    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, (size_t)n, &pkt), TFTP_OK);
    ASSERT_EQ(pkt.opcode, TFTP_OP_RRQ);
    /* filename should be truncated to TFTP_MAX_FILENAME - 1 */
    ASSERT_TRUE(strlen(pkt.u.rq.filename) <= TFTP_MAX_FILENAME - 1);
    ASSERT_STR_EQ(pkt.u.rq.mode, "octet");
}

/* ---------- error / edge cases ---------- */

TEST(test_parse_null_params)
{
    uint8_t buf[16] = {0};
    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(NULL, 16, &pkt), TFTP_ERR_PARAM);
    ASSERT_EQ(tftp_parse(buf, 16, NULL), TFTP_ERR_PARAM);
}

TEST(test_parse_short_buffer)
{
    uint8_t buf[1] = {0};
    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, 1, &pkt), TFTP_ERR_SHORT);
}

TEST(test_parse_unknown_opcode)
{
    uint8_t buf[4] = {0, 99, 0, 0};
    tftp_packet_t pkt;
    ASSERT_EQ(tftp_parse(buf, 4, &pkt), TFTP_ERR_FORMAT);
}

TEST(test_build_rrq_null_params)
{
    uint8_t buf[128];
    ASSERT_TRUE(tftp_build_rrq(NULL, 128, "f.bin", "octet") < 0);
    ASSERT_TRUE(tftp_build_rrq(buf, 128, NULL, "octet") < 0);
    ASSERT_TRUE(tftp_build_rrq(buf, 128, "f.bin", NULL) < 0);
}

TEST(test_build_rrq_empty_filename)
{
    uint8_t buf[128];
    ASSERT_TRUE(tftp_build_rrq(buf, 128, "", "octet") < 0);
}

TEST(test_build_ack_null)
{
    ASSERT_TRUE(tftp_build_ack(NULL, 16, 1) < 0);
}

TEST(test_build_ack_short)
{
    uint8_t buf[2];
    ASSERT_TRUE(tftp_build_ack(buf, sizeof(buf), 1) < 0);
}

TEST(test_build_data_null_buf)
{
    uint8_t payload[4] = {1, 2, 3, 4};
    ASSERT_TRUE(tftp_build_data(NULL, 100, 1, payload, 4) < 0);
}

TEST(test_build_data_null_data_nonzero_len)
{
    uint8_t buf[64];
    ASSERT_TRUE(tftp_build_data(buf, sizeof(buf), 1, NULL, 10) < 0);
}

TEST(test_build_data_oversized)
{
    uint8_t buf[600];
    uint8_t payload[513];
    memset(payload, 0, sizeof(payload));
    ASSERT_TRUE(tftp_build_data(buf, sizeof(buf), 1, payload, 513) < 0);
}

TEST(test_build_error_null)
{
    uint8_t buf[64];
    ASSERT_TRUE(tftp_build_error(NULL, 64, 0, "err") < 0);
    ASSERT_TRUE(tftp_build_error(buf, 64, 0, NULL) < 0);
}

TEST(test_build_rrq_short_buffer)
{
    uint8_t buf[4];
    ASSERT_TRUE(tftp_build_rrq(buf, sizeof(buf), "firmware.bin", "octet") < 0);
}

/* ---------- adversarial / fuzz ---------- */

TEST(test_parse_adversarial)
{
    tftp_packet_t pkt;

    /* all zeros */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    tftp_parse(zeros, sizeof(zeros), &pkt);

    /* all 0xFF */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    tftp_parse(ones, sizeof(ones), &pkt);

    /* RRQ with no NUL terminators */
    uint8_t bad_rrq[32];
    memset(bad_rrq, 'A', sizeof(bad_rrq));
    bad_rrq[0] = 0; bad_rrq[1] = 1; /* opcode = RRQ */
    tftp_parse(bad_rrq, sizeof(bad_rrq), &pkt);

    /* DATA with only opcode */
    uint8_t short_data[2] = {0, 3};
    tftp_parse(short_data, sizeof(short_data), &pkt);
}

TEST(test_parse_mini_fuzz)
{
    tftp_packet_t pkt;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[128];
        int len = 2 + (int)(seed % 127);
        /* set valid opcode range 1-5 so we exercise all branches */
        buf[0] = 0;
        buf[1] = (uint8_t)(1 + (seed % 6));
        for (int j = 2; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        tftp_parse(buf, (size_t)len, &pkt);
    }
    /* 1000 random inputs -- must not crash */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)TFTP_OK);
    ASSERT_EQ(-1, (int)TFTP_ERR_PARAM);
    ASSERT_EQ(-2, (int)TFTP_ERR_FORMAT);
    ASSERT_EQ(-3, (int)TFTP_ERR_SHORT);
    ASSERT_EQ(-4, (int)TFTP_ERR_OVERFLOW);
}

TEST_MAIN("tftp")
    RUN(test_build_parse_rrq);
    RUN(test_build_parse_wrq);
    RUN(test_rrq_octet_mode);
    RUN(test_build_parse_data_full);
    RUN(test_build_parse_data_partial);
    RUN(test_build_parse_data_empty);
    RUN(test_build_parse_ack);
    RUN(test_ack_block_zero);
    RUN(test_ack_block_max);
    RUN(test_build_parse_error);
    RUN(test_error_disk_full);
    RUN(test_is_last_data_full_block);
    RUN(test_is_last_data_partial);
    RUN(test_is_last_data_zero);
    RUN(test_is_last_data_non_data_pkt);
    RUN(test_is_last_data_null);
    RUN(test_large_filename_truncation);
    RUN(test_parse_null_params);
    RUN(test_parse_short_buffer);
    RUN(test_parse_unknown_opcode);
    RUN(test_build_rrq_null_params);
    RUN(test_build_rrq_empty_filename);
    RUN(test_build_ack_null);
    RUN(test_build_ack_short);
    RUN(test_build_data_null_buf);
    RUN(test_build_data_null_data_nonzero_len);
    RUN(test_build_data_oversized);
    RUN(test_build_error_null);
    RUN(test_build_rrq_short_buffer);
    RUN(test_parse_adversarial);
    RUN(test_parse_mini_fuzz);
    RUN(test_error_codes);
TEST_MAIN_END()
