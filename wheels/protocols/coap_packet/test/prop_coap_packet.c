/*
 * prop_coap_packet.c — property-based tests for coap_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_coap_packet.c ../coap_packet.c -o prop_coap_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../coap_packet.h"
#include <string.h>

/* Property: build then parse roundtrip preserves header + payload */
PROPERTY(build_parse_roundtrip, 10000) {
    coap_packet_type_t type = (coap_packet_type_t)ARBI_RANGE(seed, 0, 3);
    uint8_t code = (uint8_t)ARBI_RANGE(seed, 0, 31); /* class 0, detail 0..31 */
    uint16_t msg_id = ARBI_UINT16(seed);

    /* Token: 0..8 bytes per spec */
    uint8_t token[8];
    uint8_t token_len = (uint8_t)ARBI_RANGE(seed, 0, 8);
    ARBI_FILL(seed, token, token_len);

    /* Payload */
    uint8_t payload[128];
    uint8_t payload_len = (uint8_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, payload, payload_len);

    uint8_t buf[512];
    size_t out_len = 0;
    coap_packet_err_t err = coap_packet_build(
        buf, sizeof(buf), &out_len,
        type, code, msg_id,
        token, token_len,
        NULL, 0,  /* no options for simpler roundtrip */
        payload, payload_len);
    PROP_ASSERT_EQ(err, COAP_PACKET_OK);
    PROP_ASSERT(out_len > 0);

    coap_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    err = coap_packet_parse(buf, out_len, &pkt);
    PROP_ASSERT_EQ(err, COAP_PACKET_OK);
    PROP_ASSERT_EQ(pkt.type, type);
    PROP_ASSERT_EQ(pkt.code, code);
    PROP_ASSERT_EQ(pkt.message_id, msg_id);
    PROP_ASSERT_EQ(pkt.token_len, token_len);
    if (token_len > 0) {
        PROP_ASSERT_MEM_EQ(pkt.token, token, token_len);
    }
    PROP_ASSERT_EQ(pkt.payload_len, payload_len);
    if (payload_len > 0) {
        PROP_ASSERT_MEM_EQ(pkt.payload, payload, payload_len);
    }
}

/* Property: build with options roundtrip preserves option number + value */
PROPERTY(options_roundtrip, 5000) {
    uint8_t opt_val[16];
    uint8_t opt_len = (uint8_t)ARBI_RANGE(seed, 0, 16);
    ARBI_FILL(seed, opt_val, opt_len);

    /* Use a small option number to keep delta encoding simple */
    coap_packet_option_t opt;
    opt.number = (uint16_t)ARBI_RANGE(seed, 1, 30);
    opt.value = opt_val;
    opt.value_len = opt_len;

    uint8_t buf[256];
    size_t out_len = 0;
    coap_packet_err_t err = coap_packet_build(
        buf, sizeof(buf), &out_len,
        COAP_TYPE_CON, COAP_CODE_GET, 0x1234,
        NULL, 0,
        &opt, 1,
        NULL, 0);
    PROP_ASSERT_EQ(err, COAP_PACKET_OK);

    coap_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    err = coap_packet_parse(buf, out_len, &pkt);
    PROP_ASSERT_EQ(err, COAP_PACKET_OK);
    PROP_ASSERT_EQ(pkt.option_count, 1);
    PROP_ASSERT_EQ(pkt.options[0].number, opt.number);
    PROP_ASSERT_EQ(pkt.options[0].value_len, opt_len);
    if (opt_len > 0) {
        PROP_ASSERT_MEM_EQ(pkt.options[0].value, opt_val, opt_len);
    }
}

/* Property: build is deterministic */
PROPERTY(build_determinism, 5000) {
    uint8_t token[4];
    ARBI_FILL(seed, token, 4);
    uint8_t payload[32];
    uint8_t plen = (uint8_t)ARBI_RANGE(seed, 0, 32);
    ARBI_FILL(seed, payload, plen);

    uint8_t buf1[256], buf2[256];
    size_t len1 = 0, len2 = 0;
    coap_packet_build(buf1, sizeof(buf1), &len1,
                      COAP_TYPE_NON, COAP_CODE_POST, 0xABCD,
                      token, 4, NULL, 0, payload, plen);
    coap_packet_build(buf2, sizeof(buf2), &len2,
                      COAP_TYPE_NON, COAP_CODE_POST, 0xABCD,
                      token, 4, NULL, 0, payload, plen);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, len1);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[300];
    size_t len = ARBI_SIZE(seed, 300);
    ARBI_FILL(seed, garbage, len);

    coap_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    coap_packet_parse(garbage, len, &pkt);
}

TEST_MAIN("coap_packet properties") {
    RUN(build_parse_roundtrip);
    RUN(options_roundtrip);
    RUN(build_determinism);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
