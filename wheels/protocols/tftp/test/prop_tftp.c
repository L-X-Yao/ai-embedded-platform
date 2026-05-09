/*
 * prop_tftp.c --- property-based tests for tftp
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_tftp.c ../tftp.c -o prop_tftp
 */

#include "test_harness.h"
#include "property_test.h"
#include "../tftp.h"
#include <string.h>

/* Property: RRQ build/parse roundtrip */
PROPERTY(rrq_roundtrip, 5000) {
    uint8_t buf[600];
    char filename[32];
    size_t fn_len = ARBI_SIZE(seed, 29) + 1;
    for (size_t i = 0; i < fn_len; i++) {
        /* Printable, non-NUL chars */
        filename[i] = (char)(0x21 + ARBI_SIZE(seed, 0x5D));
    }
    filename[fn_len] = '\0';

    int len = tftp_build_rrq(buf, sizeof(buf), filename, "octet");
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    int rc = tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT_EQ(rc, TFTP_OK);
    PROP_ASSERT_EQ(pkt.opcode, TFTP_OP_RRQ);
    PROP_ASSERT(strcmp(pkt.u.rq.filename, filename) == 0);
    PROP_ASSERT(strcmp(pkt.u.rq.mode, "octet") == 0);
}

/* Property: WRQ build/parse roundtrip */
PROPERTY(wrq_roundtrip, 5000) {
    uint8_t buf[600];
    char filename[32];
    size_t fn_len = ARBI_SIZE(seed, 29) + 1;
    for (size_t i = 0; i < fn_len; i++) {
        filename[i] = (char)(0x21 + ARBI_SIZE(seed, 0x5D));
    }
    filename[fn_len] = '\0';

    int len = tftp_build_wrq(buf, sizeof(buf), filename, "netascii");
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    int rc = tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT_EQ(rc, TFTP_OK);
    PROP_ASSERT_EQ(pkt.opcode, TFTP_OP_WRQ);
    PROP_ASSERT(strcmp(pkt.u.rq.filename, filename) == 0);
    PROP_ASSERT(strcmp(pkt.u.rq.mode, "netascii") == 0);
}

/* Property: DATA build/parse roundtrip */
PROPERTY(data_roundtrip, 5000) {
    uint8_t buf[600];
    uint8_t payload[TFTP_BLOCK_SIZE];
    uint16_t block = ARBI_UINT16(seed);
    size_t payload_len = ARBI_SIZE(seed, TFTP_BLOCK_SIZE);
    ARBI_FILL(seed, payload, payload_len);

    int len = tftp_build_data(buf, sizeof(buf), block, payload, payload_len);
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    int rc = tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT_EQ(rc, TFTP_OK);
    PROP_ASSERT_EQ(pkt.opcode, TFTP_OP_DATA);
    PROP_ASSERT_EQ(pkt.u.data.block, block);
    PROP_ASSERT_EQ((long long)pkt.u.data.len, (long long)payload_len);
    if (payload_len > 0) {
        PROP_ASSERT_MEM_EQ(pkt.u.data.data, payload, payload_len);
    }
}

/* Property: ACK build/parse roundtrip */
PROPERTY(ack_roundtrip, 5000) {
    uint8_t buf[64];
    uint16_t block = ARBI_UINT16(seed);

    int len = tftp_build_ack(buf, sizeof(buf), block);
    PROP_ASSERT_EQ(len, 4);

    tftp_packet_t pkt;
    int rc = tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT_EQ(rc, TFTP_OK);
    PROP_ASSERT_EQ(pkt.opcode, TFTP_OP_ACK);
    PROP_ASSERT_EQ(pkt.u.ack.block, block);
}

/* Property: ERROR build/parse roundtrip */
PROPERTY(error_roundtrip, 5000) {
    uint8_t buf[256];
    uint16_t code = (uint16_t)ARBI_SIZE(seed, 7);
    char msg[32];
    size_t msg_len = ARBI_SIZE(seed, 29) + 1;
    for (size_t i = 0; i < msg_len; i++) {
        msg[i] = (char)(0x20 + ARBI_SIZE(seed, 0x5E));
    }
    msg[msg_len] = '\0';

    int len = tftp_build_error(buf, sizeof(buf), code, msg);
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    int rc = tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT_EQ(rc, TFTP_OK);
    PROP_ASSERT_EQ(pkt.opcode, TFTP_OP_ERROR);
    PROP_ASSERT_EQ(pkt.u.error.code, code);
    PROP_ASSERT(strcmp(pkt.u.error.msg, msg) == 0);
}

/* Property: random parse does not crash */
PROPERTY(random_parse_no_crash, 10000) {
    uint8_t buf[600];
    size_t len = ARBI_SIZE(seed, 599) + 1;
    ARBI_FILL(seed, buf, len);

    tftp_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    tftp_parse(buf, len, &pkt);
    /* No crash is success */
}

/* Property: is_last_data is consistent with data length */
PROPERTY(is_last_data_consistent, 5000) {
    uint8_t buf[600];
    uint8_t payload[TFTP_BLOCK_SIZE];
    size_t payload_len = ARBI_SIZE(seed, TFTP_BLOCK_SIZE);
    ARBI_FILL(seed, payload, payload_len);

    int len = tftp_build_data(buf, sizeof(buf), 1, payload, payload_len);
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    tftp_parse(buf, (size_t)len, &pkt);
    bool is_last = tftp_is_last_data(&pkt);

    if (payload_len < TFTP_BLOCK_SIZE) {
        PROP_ASSERT(is_last);
    } else {
        PROP_ASSERT(!is_last);
    }
}

/* Property: empty data block is the last block */
PROPERTY(empty_data_is_last, 1000) {
    uint8_t buf[64];
    int len = tftp_build_data(buf, sizeof(buf), 1, NULL, 0);
    PROP_ASSERT(len > 0);

    tftp_packet_t pkt;
    tftp_parse(buf, (size_t)len, &pkt);
    PROP_ASSERT(tftp_is_last_data(&pkt));
}

TEST_MAIN("tftp properties") {
    RUN(rrq_roundtrip);
    RUN(wrq_roundtrip);
    RUN(data_roundtrip);
    RUN(ack_roundtrip);
    RUN(error_roundtrip);
    RUN(random_parse_no_crash);
    RUN(is_last_data_consistent);
    RUN(empty_data_is_last);
}
TEST_MAIN_END()
