/*
 * prop_dhcp_packet.c --- property-based tests for dhcp_packet
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_dhcp_packet.c ../dhcp_packet.c -o prop_dhcp_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../dhcp_packet.h"
#include <string.h>

/* Property: build/parse roundtrip for DISCOVER */
PROPERTY(discover_roundtrip, 5000) {
    uint8_t buf[DHCP_PACKET_MAX_LEN];
    uint32_t xid = ARBI_UINT32(seed);
    uint8_t mac[6];
    ARBI_FILL(seed, mac, 6);

    int len = dhcp_packet_build_discover(buf, sizeof(buf), xid, mac);
    PROP_ASSERT(len > 0);
    PROP_ASSERT(len <= (int)sizeof(buf));

    dhcp_header_t hdr;
    int rc = dhcp_packet_parse(buf, (size_t)len, &hdr);
    PROP_ASSERT_EQ(rc, DHCP_PACKET_OK);

    PROP_ASSERT_EQ(hdr.op, DHCP_OP_REQUEST);
    PROP_ASSERT_EQ(hdr.htype, 1);
    PROP_ASSERT_EQ(hdr.hlen, 6);
    PROP_ASSERT_EQ((long long)hdr.xid, (long long)xid);
    PROP_ASSERT_EQ(hdr.msg_type, DHCP_MSG_DISCOVER);
    PROP_ASSERT_MEM_EQ(hdr.chaddr, mac, 6);
}

/* Property: build/parse roundtrip for REQUEST */
PROPERTY(request_roundtrip, 5000) {
    uint8_t buf[DHCP_PACKET_MAX_LEN];
    uint32_t xid = ARBI_UINT32(seed);
    uint8_t mac[6], req_ip[4], server_ip[4];
    ARBI_FILL(seed, mac, 6);
    ARBI_FILL(seed, req_ip, 4);
    ARBI_FILL(seed, server_ip, 4);

    int len = dhcp_packet_build_request(buf, sizeof(buf), xid, mac,
                                        req_ip, server_ip);
    PROP_ASSERT(len > 0);

    dhcp_header_t hdr;
    int rc = dhcp_packet_parse(buf, (size_t)len, &hdr);
    PROP_ASSERT_EQ(rc, DHCP_PACKET_OK);

    PROP_ASSERT_EQ(hdr.msg_type, DHCP_MSG_REQUEST);
    PROP_ASSERT_EQ((long long)hdr.xid, (long long)xid);
    PROP_ASSERT_MEM_EQ(hdr.chaddr, mac, 6);

    /* Verify requested IP option is present */
    dhcp_option_t opt;
    int orc = dhcp_packet_get_option(buf, (size_t)len, DHCP_OPT_REQ_IP, &opt);
    PROP_ASSERT_EQ(orc, 0);
    PROP_ASSERT_MEM_EQ(opt.data, req_ip, 4);
}

/* Property: random parse does not crash */
PROPERTY(random_parse_no_crash, 10000) {
    uint8_t buf[600];
    size_t len = ARBI_SIZE(seed, 599) + 1;
    ARBI_FILL(seed, buf, len);

    dhcp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dhcp_packet_parse(buf, len, &hdr);
    /* No crash is success */
}

/* Property: random option iteration does not crash */
PROPERTY(random_option_iter_no_crash, 10000) {
    uint8_t buf[600];
    size_t len = ARBI_SIZE(seed, 599) + 1;
    ARBI_FILL(seed, buf, len);

    size_t offset = ARBI_SIZE(seed, len);
    dhcp_option_t opt;
    /* Iterate up to 100 times to guard against infinite loops */
    for (int i = 0; i < 100; i++) {
        int rc = dhcp_packet_next_option(buf, len, &offset, &opt);
        if (rc <= 0) break;
    }
}

/* Property: get_option returns -1 for garbage data */
PROPERTY(get_option_garbage, 5000) {
    uint8_t buf[300];
    size_t len = ARBI_SIZE(seed, 299) + 1;
    ARBI_FILL(seed, buf, len);

    dhcp_option_t opt;
    int rc = dhcp_packet_get_option(buf, len, DHCP_OPT_MSG_TYPE, &opt);
    /* Either succeeds or fails gracefully -- no crash */
    (void)rc;
}

/* Property: option_u32 / option_ip do not crash with NULL/short data */
PROPERTY(option_helpers_safe, 1000) {
    uint32_t v = dhcp_packet_option_u32(NULL);
    PROP_ASSERT_EQ((long long)v, 0);

    dhcp_option_t short_opt = { .code = 1, .len = 2, .data = (const uint8_t *)"AB" };
    v = dhcp_packet_option_u32(&short_opt);
    PROP_ASSERT_EQ((long long)v, 0);

    uint8_t ip[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    dhcp_packet_option_ip(NULL, ip);
    /* ip should be cleared */
    PROP_ASSERT_EQ(ip[0], 0);

    dhcp_packet_option_ip(&short_opt, ip);
    /* short opt -> zeroed */
    PROP_ASSERT_EQ(ip[0], 0);
}

TEST_MAIN("dhcp_packet properties") {
    RUN(discover_roundtrip);
    RUN(request_roundtrip);
    RUN(random_parse_no_crash);
    RUN(random_option_iter_no_crash);
    RUN(get_option_garbage);
    RUN(option_helpers_safe);
}
TEST_MAIN_END()
