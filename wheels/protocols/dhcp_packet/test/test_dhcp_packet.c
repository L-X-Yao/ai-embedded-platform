// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "dhcp_packet.h"
#include <string.h>

/* ---------- helper: build a minimal OFFER packet ---------- */

static int build_offer(uint8_t *buf, size_t cap, uint32_t xid,
                       const uint8_t mac[6], const uint8_t yiaddr[4],
                       const uint8_t server_ip[4], uint32_t lease_time)
{
    if (cap < 280) return -1;
    memset(buf, 0, 280);

    buf[0] = DHCP_OP_REPLY;       /* op */
    buf[1] = 1;                    /* htype = Ethernet */
    buf[2] = 6;                    /* hlen */
    /* xid big-endian */
    buf[4] = (uint8_t)(xid >> 24);
    buf[5] = (uint8_t)(xid >> 16);
    buf[6] = (uint8_t)(xid >> 8);
    buf[7] = (uint8_t)(xid);
    /* yiaddr */
    memcpy(buf + 16, yiaddr, 4);
    /* siaddr */
    memcpy(buf + 20, server_ip, 4);
    /* chaddr */
    memcpy(buf + 28, mac, 6);
    /* magic cookie at 236 */
    buf[236] = 0x63; buf[237] = 0x82; buf[238] = 0x53; buf[239] = 0x63;

    size_t pos = 240;
    /* option 53: msg type = OFFER */
    buf[pos++] = 53; buf[pos++] = 1; buf[pos++] = DHCP_MSG_OFFER;
    /* option 54: server id */
    buf[pos++] = 54; buf[pos++] = 4;
    memcpy(buf + pos, server_ip, 4); pos += 4;
    /* option 51: lease time */
    buf[pos++] = 51; buf[pos++] = 4;
    buf[pos++] = (uint8_t)(lease_time >> 24);
    buf[pos++] = (uint8_t)(lease_time >> 16);
    buf[pos++] = (uint8_t)(lease_time >> 8);
    buf[pos++] = (uint8_t)(lease_time);
    /* option 1: subnet mask */
    buf[pos++] = 1; buf[pos++] = 4;
    buf[pos++] = 255; buf[pos++] = 255; buf[pos++] = 255; buf[pos++] = 0;
    /* option 3: router */
    buf[pos++] = 3; buf[pos++] = 4;
    buf[pos++] = 192; buf[pos++] = 168; buf[pos++] = 1; buf[pos++] = 1;
    /* END */
    buf[pos++] = 255;

    return (int)pos;
}

/* ---------- helper: build a minimal ACK packet ---------- */

static int build_ack(uint8_t *buf, size_t cap, uint32_t xid,
                     const uint8_t mac[6], const uint8_t yiaddr[4],
                     const uint8_t server_ip[4], uint32_t lease_time)
{
    int n = build_offer(buf, cap, xid, mac, yiaddr, server_ip, lease_time);
    if (n < 0) return n;
    /* change msg_type option value from OFFER(2) to ACK(5) */
    buf[242] = DHCP_MSG_ACK;
    return n;
}

/* ---------- parse tests ---------- */

TEST(test_parse_discover)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 0x12345678, mac);
    ASSERT_TRUE(n > 0);

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.op, DHCP_OP_REQUEST);
    ASSERT_EQ(hdr.htype, 1);
    ASSERT_EQ(hdr.hlen, 6);
    ASSERT_EQ(hdr.xid, 0x12345678U);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_DISCOVER);
    ASSERT_MEMEQ_N(hdr.chaddr, mac, 6);
}

TEST(test_parse_offer)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint8_t yip[4] = {192, 168, 1, 100};
    const uint8_t sip[4] = {192, 168, 1, 1};
    int n = build_offer(buf, sizeof(buf), 0xDEADBEEF, mac, yip, sip, 3600);
    ASSERT_TRUE(n > 0);

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.op, DHCP_OP_REPLY);
    ASSERT_EQ(hdr.xid, 0xDEADBEEFU);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_OFFER);
    ASSERT_MEMEQ_N(hdr.yiaddr, yip, 4);
    ASSERT_MEMEQ_N(hdr.siaddr, sip, 4);
}

TEST(test_parse_ack)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint8_t yip[4] = {10, 0, 0, 50};
    const uint8_t sip[4] = {10, 0, 0, 1};
    int n = build_ack(buf, sizeof(buf), 0xCAFEBABE, mac, yip, sip, 7200);
    ASSERT_TRUE(n > 0);

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.op, DHCP_OP_REPLY);
    ASSERT_EQ(hdr.xid, 0xCAFEBABEU);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_ACK);
    ASSERT_MEMEQ_N(hdr.yiaddr, yip, 4);
}

/* ---------- build tests ---------- */

TEST(test_build_discover_fields)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 0xAABBCCDD, mac);
    ASSERT_TRUE(n > 0);

    /* op = REQUEST(1), htype = 1, hlen = 6 */
    ASSERT_EQ(buf[0], 1);
    ASSERT_EQ(buf[1], 1);
    ASSERT_EQ(buf[2], 6);

    /* xid in network byte order */
    ASSERT_EQ(buf[4], 0xAA);
    ASSERT_EQ(buf[5], 0xBB);
    ASSERT_EQ(buf[6], 0xCC);
    ASSERT_EQ(buf[7], 0xDD);

    /* flags: broadcast */
    ASSERT_EQ(buf[10], 0x80);
    ASSERT_EQ(buf[11], 0x00);

    /* chaddr */
    ASSERT_MEMEQ_N(buf + 28, mac, 6);

    /* magic cookie */
    ASSERT_EQ(buf[236], 0x63);
    ASSERT_EQ(buf[237], 0x82);
    ASSERT_EQ(buf[238], 0x53);
    ASSERT_EQ(buf[239], 0x63);

    /* first option: msg type */
    ASSERT_EQ(buf[240], DHCP_OPT_MSG_TYPE);
    ASSERT_EQ(buf[241], 1);
    ASSERT_EQ(buf[242], DHCP_MSG_DISCOVER);
}

TEST(test_build_request_fields)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    const uint8_t req_ip[4] = {192, 168, 10, 50};
    const uint8_t srv_ip[4] = {192, 168, 10, 1};
    int n = dhcp_packet_build_request(buf, sizeof(buf), 0x11223344,
                                      mac, req_ip, srv_ip);
    ASSERT_TRUE(n > 0);

    /* parse back to verify */
    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_REQUEST);
    ASSERT_EQ(hdr.xid, 0x11223344U);

    /* check requested IP option */
    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_REQ_IP, &opt), 0);
    ASSERT_EQ(opt.len, 4);
    ASSERT_MEMEQ_N(opt.data, req_ip, 4);

    /* check server ID option */
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_SERVER_ID, &opt), 0);
    ASSERT_EQ(opt.len, 4);
    ASSERT_MEMEQ_N(opt.data, srv_ip, 4);
}

/* ---------- option iteration ---------- */

TEST(test_option_iterate)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 1, mac);
    ASSERT_TRUE(n > 0);

    size_t offset = 240;
    dhcp_option_t opt;
    int count = 0;
    int rc;

    while ((rc = dhcp_packet_next_option(buf, (size_t)n, &offset, &opt)) == 1)
    {
        count++;
    }
    ASSERT_EQ(rc, 0);  /* ended cleanly */
    ASSERT_TRUE(count >= 2);  /* at least msg_type + param_list */
}

TEST(test_get_specific_option)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 0x42, mac);
    ASSERT_TRUE(n > 0);

    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_MSG_TYPE, &opt), 0);
    ASSERT_EQ(opt.code, DHCP_OPT_MSG_TYPE);
    ASSERT_EQ(opt.len, 1);
    ASSERT_EQ(opt.data[0], DHCP_MSG_DISCOVER);
}

TEST(test_get_option_not_found)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 1, mac);
    ASSERT_TRUE(n > 0);

    dhcp_option_t opt;
    /* hostname not present in DISCOVER */
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_HOSTNAME, &opt), -1);
}

/* ---------- option value helpers ---------- */

TEST(test_option_get_u32)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint8_t yip[4] = {192, 168, 1, 100};
    const uint8_t sip[4] = {192, 168, 1, 1};
    int n = build_offer(buf, sizeof(buf), 0x1, mac, yip, sip, 3600);
    ASSERT_TRUE(n > 0);

    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_LEASE_TIME, &opt), 0);
    ASSERT_EQ(dhcp_packet_option_u32(&opt), 3600U);
}

TEST(test_option_get_ip)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint8_t yip[4] = {192, 168, 1, 100};
    const uint8_t sip[4] = {192, 168, 1, 1};
    int n = build_offer(buf, sizeof(buf), 0x1, mac, yip, sip, 3600);
    ASSERT_TRUE(n > 0);

    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_SUBNET_MASK, &opt), 0);

    uint8_t ip[4];
    dhcp_packet_option_ip(&opt, ip);
    ASSERT_EQ(ip[0], 255);
    ASSERT_EQ(ip[1], 255);
    ASSERT_EQ(ip[2], 255);
    ASSERT_EQ(ip[3], 0);
}

TEST(test_option_get_router_ip)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint8_t yip[4] = {192, 168, 1, 100};
    const uint8_t sip[4] = {192, 168, 1, 1};
    int n = build_offer(buf, sizeof(buf), 0x1, mac, yip, sip, 3600);
    ASSERT_TRUE(n > 0);

    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_get_option(buf, (size_t)n, DHCP_OPT_ROUTER, &opt), 0);

    uint8_t ip[4];
    dhcp_packet_option_ip(&opt, ip);
    ASSERT_EQ(ip[0], 192);
    ASSERT_EQ(ip[1], 168);
    ASSERT_EQ(ip[2], 1);
    ASSERT_EQ(ip[3], 1);
}

/* ---------- error / edge cases ---------- */

TEST(test_parse_null_params)
{
    uint8_t buf[300];
    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(NULL, 300, &hdr), DHCP_PACKET_ERR_PARAM);
    ASSERT_EQ(dhcp_packet_parse(buf, 300, NULL), DHCP_PACKET_ERR_PARAM);
}

TEST(test_parse_short_buffer)
{
    uint8_t buf[100];
    memset(buf, 0, sizeof(buf));
    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, sizeof(buf), &hdr), DHCP_PACKET_ERR_SHORT);
}

TEST(test_parse_missing_magic_cookie)
{
    /* build a buffer that is DHCP_PACKET_MIN_LEN but with wrong cookie */
    uint8_t buf[300];
    memset(buf, 0, sizeof(buf));
    buf[0] = 1;  /* op */
    buf[1] = 1;  /* htype */
    buf[2] = 6;  /* hlen */
    /* cookie at 236: leave as zeros (wrong) */

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, 280, &hdr), DHCP_PACKET_ERR_FORMAT);
}

TEST(test_build_discover_short_buffer)
{
    uint8_t buf[100];
    const uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 1, mac);
    ASSERT_TRUE(n < 0);
}

TEST(test_build_request_null_params)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t ip[4] = {10, 0, 0, 1};

    ASSERT_TRUE(dhcp_packet_build_request(NULL, 576, 1, mac, ip, ip) < 0);
    ASSERT_TRUE(dhcp_packet_build_request(buf, 576, 1, NULL, ip, ip) < 0);
    ASSERT_TRUE(dhcp_packet_build_request(buf, 576, 1, mac, NULL, ip) < 0);
    ASSERT_TRUE(dhcp_packet_build_request(buf, 576, 1, mac, ip, NULL) < 0);
}

TEST(test_option_get_u32_null)
{
    ASSERT_EQ(dhcp_packet_option_u32(NULL), 0U);

    dhcp_option_t opt;
    opt.data = NULL;
    opt.len = 4;
    ASSERT_EQ(dhcp_packet_option_u32(&opt), 0U);
}

TEST(test_option_get_ip_null)
{
    uint8_t ip[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    dhcp_packet_option_ip(NULL, ip);
    /* ip should be zeroed when opt is NULL */
    ASSERT_EQ(ip[0], 0);

    /* NULL ip pointer should not crash */
    dhcp_option_t opt;
    opt.data = ip;
    opt.len = 4;
    dhcp_packet_option_ip(&opt, NULL);
}

TEST(test_next_option_null)
{
    ASSERT_EQ(dhcp_packet_next_option(NULL, 10, NULL, NULL), -1);

    size_t off = 0;
    dhcp_option_t opt;
    ASSERT_EQ(dhcp_packet_next_option(NULL, 10, &off, &opt), -1);
}

/* ---------- build + parse roundtrip ---------- */

TEST(test_discover_roundtrip)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45};
    int n = dhcp_packet_build_discover(buf, sizeof(buf), 0x87654321, mac);
    ASSERT_TRUE(n > 0);

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.op, DHCP_OP_REQUEST);
    ASSERT_EQ(hdr.xid, 0x87654321U);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_DISCOVER);
    ASSERT_MEMEQ_N(hdr.chaddr, mac, 6);
}

TEST(test_request_roundtrip)
{
    uint8_t buf[576];
    const uint8_t mac[6] = {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54};
    const uint8_t req_ip[4] = {172, 16, 0, 42};
    const uint8_t srv_ip[4] = {172, 16, 0, 1};
    int n = dhcp_packet_build_request(buf, sizeof(buf), 0xFEDCBA98,
                                      mac, req_ip, srv_ip);
    ASSERT_TRUE(n > 0);

    dhcp_header_t hdr;
    ASSERT_EQ(dhcp_packet_parse(buf, (size_t)n, &hdr), DHCP_PACKET_OK);
    ASSERT_EQ(hdr.msg_type, DHCP_MSG_REQUEST);
    ASSERT_EQ(hdr.xid, 0xFEDCBA98U);
}

/* ---------- adversarial / fuzz ---------- */

TEST(test_parse_adversarial)
{
    dhcp_header_t hdr;
    /* all zeros (wrong cookie) */
    uint8_t zeros[300];
    memset(zeros, 0, sizeof(zeros));
    dhcp_packet_parse(zeros, sizeof(zeros), &hdr);

    /* all 0xFF */
    uint8_t ones[300];
    memset(ones, 0xFF, sizeof(ones));
    dhcp_packet_parse(ones, sizeof(ones), &hdr);

    /* valid cookie but truncated options */
    uint8_t trunc[244];
    memset(trunc, 0, sizeof(trunc));
    trunc[236] = 0x63; trunc[237] = 0x82;
    trunc[238] = 0x53; trunc[239] = 0x63;
    trunc[240] = 53;   trunc[241] = 1;  /* option 53, len=1, but no data after */
    /* should not crash */
    dhcp_packet_parse(trunc, 242, &hdr);
}

TEST(test_parse_mini_fuzz)
{
    dhcp_header_t hdr;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[300];
        int len = 240 + (int)(seed % 61);
        /* put valid cookie so we exercise option parsing */
        memset(buf, 0, (size_t)len);
        buf[236] = 0x63; buf[237] = 0x82;
        buf[238] = 0x53; buf[239] = 0x63;
        for (int j = 240; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        dhcp_packet_parse(buf, (size_t)len, &hdr);
    }
    /* 1000 random option payloads -- must not crash */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)DHCP_PACKET_OK);
    ASSERT_EQ(-1, (int)DHCP_PACKET_ERR_PARAM);
    ASSERT_EQ(-2, (int)DHCP_PACKET_ERR_FORMAT);
    ASSERT_EQ(-3, (int)DHCP_PACKET_ERR_SHORT);
    ASSERT_EQ(-4, (int)DHCP_PACKET_ERR_OVERFLOW);
}

TEST_MAIN("dhcp_packet")
    RUN(test_parse_discover);
    RUN(test_parse_offer);
    RUN(test_parse_ack);
    RUN(test_build_discover_fields);
    RUN(test_build_request_fields);
    RUN(test_option_iterate);
    RUN(test_get_specific_option);
    RUN(test_get_option_not_found);
    RUN(test_option_get_u32);
    RUN(test_option_get_ip);
    RUN(test_option_get_router_ip);
    RUN(test_parse_null_params);
    RUN(test_parse_short_buffer);
    RUN(test_parse_missing_magic_cookie);
    RUN(test_build_discover_short_buffer);
    RUN(test_build_request_null_params);
    RUN(test_option_get_u32_null);
    RUN(test_option_get_ip_null);
    RUN(test_next_option_null);
    RUN(test_discover_roundtrip);
    RUN(test_request_roundtrip);
    RUN(test_parse_adversarial);
    RUN(test_parse_mini_fuzz);
    RUN(test_error_codes);
TEST_MAIN_END()
