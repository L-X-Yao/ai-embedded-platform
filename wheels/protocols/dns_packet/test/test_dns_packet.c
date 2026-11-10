// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "dns_packet.h"
#include <string.h>

/* ---------- dns_build_query ---------- */

TEST(test_build_query_a_record)
{
    uint8_t buf[128];
    int n = dns_build_query(buf, sizeof(buf), 0x1234, "example.com", DNS_TYPE_A);
    ASSERT_TRUE(n > 0);

    /* header: ID=0x1234, flags=0x0100 (RD), QDCOUNT=1 */
    ASSERT_EQ(buf[0], 0x12);
    ASSERT_EQ(buf[1], 0x34);
    ASSERT_EQ(buf[2], 0x01);  /* RD=1 */
    ASSERT_EQ(buf[3], 0x00);
    ASSERT_EQ(buf[4], 0x00);  /* QDCOUNT high */
    ASSERT_EQ(buf[5], 0x01);  /* QDCOUNT low */

    /* question: 7"example"3"com"0 + QTYPE(A=1) + QCLASS(IN=1) */
    /* total = 12 + (1+7+1+3+1) + 4 = 29 */
    ASSERT_EQ(n, 29);
}

TEST(test_build_query_aaaa_type)
{
    uint8_t buf[128];
    int n = dns_build_query(buf, sizeof(buf), 0xABCD, "test.io", DNS_TYPE_AAAA);
    ASSERT_TRUE(n > 0);

    /* find qtype at end: last 4 bytes are QTYPE(2) + QCLASS(2) */
    uint16_t qtype = (uint16_t)((uint16_t)buf[n - 4] << 8 | buf[n - 3]);
    uint16_t qclass = (uint16_t)((uint16_t)buf[n - 2] << 8 | buf[n - 1]);
    ASSERT_EQ(qtype, DNS_TYPE_AAAA);
    ASSERT_EQ(qclass, DNS_CLASS_IN);
}

TEST(test_build_query_buffer_too_small)
{
    uint8_t buf[4];
    int n = dns_build_query(buf, sizeof(buf), 0x0001, "example.com", DNS_TYPE_A);
    ASSERT_TRUE(n < 0);
}

TEST(test_build_query_null_params)
{
    uint8_t buf[128];
    ASSERT_EQ(dns_build_query(NULL, 128, 1, "x.com", DNS_TYPE_A), DNS_ERR_PARAM);
    ASSERT_EQ(dns_build_query(buf, 128, 1, NULL, DNS_TYPE_A), DNS_ERR_PARAM);
}

/* ---------- dns_encode_name ---------- */

TEST(test_encode_name_simple)
{
    uint8_t buf[64];
    int n = dns_encode_name("example.com", buf, sizeof(buf));
    ASSERT_EQ(n, 13);  /* 1+7+1+3+1 = 13 */

    /* 7 "example" */
    ASSERT_EQ(buf[0], 7);
    ASSERT_MEMEQ(buf + 1, "example");
    /* 3 "com" */
    ASSERT_EQ(buf[8], 3);
    ASSERT_MEMEQ(buf + 9, "com");
    /* null terminator */
    ASSERT_EQ(buf[12], 0);
}

TEST(test_encode_name_multi_label)
{
    uint8_t buf[64];
    int n = dns_encode_name("sub.domain.example.org", buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    /* 3"sub" 6"domain" 7"example" 3"org" 0 */
    ASSERT_EQ(buf[0], 3);
    ASSERT_MEMEQ(buf + 1, "sub");
    ASSERT_EQ(buf[4], 6);
    ASSERT_MEMEQ(buf + 5, "domain");
    ASSERT_EQ(buf[11], 7);
    ASSERT_MEMEQ(buf + 12, "example");
    ASSERT_EQ(buf[19], 3);
    ASSERT_MEMEQ(buf + 20, "org");
    ASSERT_EQ(buf[23], 0);
    ASSERT_EQ(n, 24);
}

TEST(test_encode_name_long_domain)
{
    /* label > 63 chars should fail */
    char long_name[128];
    memset(long_name, 'a', 64);
    long_name[64] = '.';
    long_name[65] = 'b';
    long_name[66] = '\0';
    uint8_t buf[256];
    ASSERT_EQ(dns_encode_name(long_name, buf, sizeof(buf)), DNS_ERR_FORMAT);
}

/* ---------- dns_decode_name ---------- */

TEST(test_decode_name_simple)
{
    /* 7"example"3"com"0 */
    uint8_t pkt[] = {
        7, 'e','x','a','m','p','l','e',
        3, 'c','o','m',
        0
    };
    char name[64];
    int consumed = dns_decode_name(pkt, sizeof(pkt), 0, name, sizeof(name));
    ASSERT_EQ(consumed, 13);
    ASSERT_EQ(strcmp(name, "example.com"), 0);
}

TEST(test_decode_name_pointer_compression)
{
    /*
     * Simulate a packet with pointer compression:
     * offset 0:  7"example"3"com"0        (13 bytes, the original name)
     * offset 13: 0xC0 0x00                (pointer to offset 0)
     */
    uint8_t pkt[15];
    /* original name at offset 0 */
    pkt[0] = 7;
    memcpy(pkt + 1, "example", 7);
    pkt[8] = 3;
    memcpy(pkt + 9, "com", 3);
    pkt[12] = 0;
    /* pointer at offset 13 */
    pkt[13] = 0xC0;
    pkt[14] = 0x00;

    char name[64];
    int consumed = dns_decode_name(pkt, sizeof(pkt), 13, name, sizeof(name));
    ASSERT_EQ(consumed, 2);   /* pointer is 2 bytes */
    ASSERT_EQ(strcmp(name, "example.com"), 0);
}

/* ---------- dns_parse_header ---------- */

TEST(test_parse_header)
{
    /* Build a query and parse its header */
    uint8_t buf[128];
    int n = dns_build_query(buf, sizeof(buf), 0x5678, "test.io", DNS_TYPE_A);
    ASSERT_TRUE(n > 0);

    dns_header_t hdr;
    ASSERT_EQ(dns_parse_header(buf, (uint16_t)n, &hdr), DNS_OK);
    ASSERT_EQ(hdr.id, 0x5678);
    ASSERT_FALSE(hdr.is_response);
    ASSERT_EQ(hdr.rcode, 0);
    ASSERT_EQ(hdr.qdcount, 1);
    ASSERT_EQ(hdr.ancount, 0);
}

TEST(test_parse_header_too_short)
{
    uint8_t buf[4] = {0};
    dns_header_t hdr;
    ASSERT_EQ(dns_parse_header(buf, 4, &hdr), DNS_ERR_SIZE);
}

/* ---------- dns_parse_record (question) ---------- */

TEST(test_parse_question_record)
{
    uint8_t buf[128];
    int n = dns_build_query(buf, sizeof(buf), 0x0001, "example.com", DNS_TYPE_A);
    ASSERT_TRUE(n > 0);

    dns_record_t rec;
    int new_offset = dns_parse_record(buf, (uint16_t)n, DNS_HEADER_SIZE,
                                      &rec, false);
    ASSERT_TRUE(new_offset > 0);
    ASSERT_EQ(new_offset, n);   /* should consume entire question */
    ASSERT_EQ(strcmp(rec.name, "example.com"), 0);
    ASSERT_EQ(rec.type, DNS_TYPE_A);
    ASSERT_EQ(rec.cls, DNS_CLASS_IN);
}

/* ---------- dns_parse_record (answer with A record) ---------- */

TEST(test_parse_answer_a_record)
{
    /*
     * Construct a minimal DNS response with 1 answer:
     *   Header: ID=0x1234, QR=1, QDCOUNT=0, ANCOUNT=1
     *   Answer: name=7"example"3"com"0, TYPE=A, CLASS=IN,
     *           TTL=300, RDLENGTH=4, RDATA=93.184.216.34
     */
    uint8_t pkt[64];
    memset(pkt, 0, sizeof(pkt));

    /* header */
    pkt[0] = 0x12; pkt[1] = 0x34;   /* ID */
    pkt[2] = 0x81; pkt[3] = 0x80;   /* QR=1, RD=1, RA=1 */
    pkt[4] = 0x00; pkt[5] = 0x00;   /* QDCOUNT=0 */
    pkt[6] = 0x00; pkt[7] = 0x01;   /* ANCOUNT=1 */

    /* answer RR at offset 12 */
    uint16_t pos = 12;

    /* name: 7"example"3"com"0 */
    pkt[pos++] = 7;
    memcpy(pkt + pos, "example", 7); pos += 7;
    pkt[pos++] = 3;
    memcpy(pkt + pos, "com", 3); pos += 3;
    pkt[pos++] = 0;

    /* TYPE = A (1) */
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    /* CLASS = IN (1) */
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    /* TTL = 300 */
    pkt[pos++] = 0x00; pkt[pos++] = 0x00;
    pkt[pos++] = 0x01; pkt[pos++] = 0x2C;
    /* RDLENGTH = 4 */
    pkt[pos++] = 0x00; pkt[pos++] = 0x04;
    /* RDATA: 93.184.216.34 */
    pkt[pos++] = 93; pkt[pos++] = 184;
    pkt[pos++] = 216; pkt[pos++] = 34;

    dns_record_t rec;
    int new_off = dns_parse_record(pkt, pos, 12, &rec, true);
    ASSERT_TRUE(new_off > 0);
    ASSERT_EQ(new_off, pos);
    ASSERT_EQ(strcmp(rec.name, "example.com"), 0);
    ASSERT_EQ(rec.type, DNS_TYPE_A);
    ASSERT_EQ(rec.cls, DNS_CLASS_IN);
    ASSERT_EQ(rec.ttl, 300);
    ASSERT_EQ(rec.rdlength, 4);
    ASSERT_TRUE(rec.rdata != NULL);
}

/* ---------- dns_get_ipv4 ---------- */

TEST(test_get_ipv4)
{
    /* Reuse the answer packet from above */
    uint8_t pkt[64];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x12; pkt[1] = 0x34;
    pkt[2] = 0x81; pkt[3] = 0x80;
    pkt[6] = 0x00; pkt[7] = 0x01;

    uint16_t pos = 12;
    pkt[pos++] = 7;
    memcpy(pkt + pos, "example", 7); pos += 7;
    pkt[pos++] = 3;
    memcpy(pkt + pos, "com", 3); pos += 3;
    pkt[pos++] = 0;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;  /* TYPE A */
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;  /* CLASS IN */
    pkt[pos++] = 0x00; pkt[pos++] = 0x00;
    pkt[pos++] = 0x01; pkt[pos++] = 0x2C;  /* TTL 300 */
    pkt[pos++] = 0x00; pkt[pos++] = 0x04;  /* RDLENGTH 4 */
    pkt[pos++] = 93; pkt[pos++] = 184;
    pkt[pos++] = 216; pkt[pos++] = 34;

    dns_record_t rec;
    int new_off = dns_parse_record(pkt, pos, 12, &rec, true);
    ASSERT_TRUE(new_off > 0);

    uint8_t ip[4];
    ASSERT_EQ(dns_get_ipv4(&rec, ip), DNS_OK);
    ASSERT_EQ(ip[0], 93);
    ASSERT_EQ(ip[1], 184);
    ASSERT_EQ(ip[2], 216);
    ASSERT_EQ(ip[3], 34);
}

/* ---------- NXDOMAIN response ---------- */

TEST(test_nxdomain_response)
{
    uint8_t pkt[12];
    memset(pkt, 0, 12);
    pkt[0] = 0xAA; pkt[1] = 0xBB;
    pkt[2] = 0x81; pkt[3] = 0x83;  /* QR=1, RCODE=3 (NXDOMAIN) */

    dns_header_t hdr;
    ASSERT_EQ(dns_parse_header(pkt, 12, &hdr), DNS_OK);
    ASSERT_EQ(hdr.id, 0xAABB);
    ASSERT_TRUE(hdr.is_response);
    ASSERT_EQ(hdr.rcode, 3);
}

/* ---------- build + parse roundtrip ---------- */

TEST(test_build_parse_roundtrip)
{
    uint8_t buf[128];
    uint16_t id = 0x9999;
    int n = dns_build_query(buf, sizeof(buf), id, "api.example.org", DNS_TYPE_A);
    ASSERT_TRUE(n > 0);

    /* parse header */
    dns_header_t hdr;
    ASSERT_EQ(dns_parse_header(buf, (uint16_t)n, &hdr), DNS_OK);
    ASSERT_EQ(hdr.id, id);
    ASSERT_FALSE(hdr.is_response);
    ASSERT_EQ(hdr.qdcount, 1);

    /* parse question */
    dns_record_t rec;
    int end = dns_parse_record(buf, (uint16_t)n, DNS_HEADER_SIZE, &rec, false);
    ASSERT_EQ(end, n);
    ASSERT_EQ(strcmp(rec.name, "api.example.org"), 0);
    ASSERT_EQ(rec.type, DNS_TYPE_A);
    ASSERT_EQ(rec.cls, DNS_CLASS_IN);
}

/* RFC 1035 3.1: a domain name is terminated by a length byte of zero. */

TEST(test_decode_pointer_to_a_name_with_no_root_label)
{
    /* Pointer at 12 to the label at 14, which ends the 18-byte packet */
    uint8_t pkt[18];
    memset(pkt, 0, sizeof(pkt));
    pkt[12] = 0xC0;
    pkt[13] = 0x0E;
    pkt[14] = 3;
    memcpy(pkt + 15, "abc", 3);

    char name[32];
    ASSERT_EQ(dns_decode_name(pkt, sizeof(pkt), 12, name, sizeof(name)),
              DNS_ERR_FORMAT);
}

TEST(test_decode_labels_then_pointer_to_a_name_with_no_root_label)
{
    /* x at 12, then a pointer at 14 to the label at 16 */
    uint8_t pkt[20];
    memset(pkt, 0, sizeof(pkt));
    pkt[12] = 1;
    pkt[13] = 'x';
    pkt[14] = 0xC0;
    pkt[15] = 0x10;
    pkt[16] = 3;
    memcpy(pkt + 17, "abc", 3);

    char name[32];
    ASSERT_EQ(dns_decode_name(pkt, sizeof(pkt), 12, name, sizeof(name)),
              DNS_ERR_FORMAT);
}

TEST(test_decode_label_with_no_root_label)
{
    uint8_t pkt[4] = { 3, 'a', 'b', 'c' };

    char name[32];
    ASSERT_EQ(dns_decode_name(pkt, sizeof(pkt), 0, name, sizeof(name)),
              DNS_ERR_FORMAT);
}

TEST(test_decode_pointer_to_a_terminated_name)
{
    /* Same as the first case, with the root label present */
    uint8_t pkt[19];
    memset(pkt, 0, sizeof(pkt));
    pkt[12] = 0xC0;
    pkt[13] = 0x0E;
    pkt[14] = 3;
    memcpy(pkt + 15, "abc", 3);

    char name[32];
    ASSERT_EQ(dns_decode_name(pkt, sizeof(pkt), 12, name, sizeof(name)), 2);
    ASSERT_EQ(strcmp(name, "abc"), 0);
}

/* RFC 1035 section 3.2.2 and 3.2.4: TYPE and CLASS are 16 bits on the wire.
   arm-none-eabi-gcc makes an enum as narrow as its values allow, so an enum
   field would cut 257 down to 1. CMake also builds this file with
   -fshort-enums to see what the target sees. */
static const uint8_t a_record_head[] = {
    0x12, 0x34, 0x81, 0x80, 0, 1, 0, 1, 0, 0, 0, 0, 1, 'a', 0,
};

static size_t build_answer(uint8_t *pkt, uint16_t type, uint16_t cls)
{
    size_t n = sizeof(a_record_head);
    memcpy(pkt, a_record_head, n);
    pkt[n++] = (uint8_t)(type >> 8);
    pkt[n++] = (uint8_t)type;
    pkt[n++] = (uint8_t)(cls >> 8);
    pkt[n++] = (uint8_t)cls;
    memcpy(pkt + n, "\0\0\0\x3C\0\x04\x01\x02\x03\x04", 10);
    return n + 10;
}

TEST(test_record_fields_hold_16_bits)
{
    dns_record_t rec;
    ASSERT_EQ(sizeof(rec.type), 2U);
    ASSERT_EQ(sizeof(rec.cls), 2U);
}

TEST(test_record_keeps_a_type_past_one_byte)
{
    static const uint16_t types[] = { 257, 256 + 28, 0x0100, 0xFFFF };
    uint8_t pkt[64];
    dns_record_t rec;
    uint8_t ip[4];

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        size_t n = build_answer(pkt, types[i], DNS_CLASS_IN);
        ASSERT_EQ(dns_parse_record(pkt, (uint16_t)n, 12, &rec, true), (int)n);
        ASSERT_EQ(rec.type, types[i]);
        ASSERT_EQ(dns_get_ipv4(&rec, ip), DNS_ERR_FORMAT);
    }
}

TEST(test_record_keeps_a_class_past_one_byte)
{
    uint8_t pkt[64];
    dns_record_t rec;
    size_t n = build_answer(pkt, DNS_TYPE_A, 257);
    ASSERT_EQ(dns_parse_record(pkt, (uint16_t)n, 12, &rec, true), (int)n);
    ASSERT_EQ(rec.cls, 257);
    ASSERT_TRUE(rec.cls != DNS_CLASS_IN);
}

TEST(test_a_record_still_gives_its_address)
{
    uint8_t pkt[64];
    dns_record_t rec;
    uint8_t ip[4];
    size_t n = build_answer(pkt, DNS_TYPE_A, DNS_CLASS_IN);
    ASSERT_EQ(dns_parse_record(pkt, (uint16_t)n, 12, &rec, true), (int)n);
    ASSERT_EQ(rec.type, DNS_TYPE_A);
    ASSERT_EQ(rec.cls, DNS_CLASS_IN);
    ASSERT_EQ(dns_get_ipv4(&rec, ip), DNS_OK);
    ASSERT_EQ(ip[3], 4);
}

TEST_MAIN("dns_packet")
    RUN(test_build_query_a_record);
    RUN(test_build_query_aaaa_type);
    RUN(test_build_query_buffer_too_small);
    RUN(test_build_query_null_params);
    RUN(test_encode_name_simple);
    RUN(test_encode_name_multi_label);
    RUN(test_encode_name_long_domain);
    RUN(test_decode_name_simple);
    RUN(test_decode_name_pointer_compression);
    RUN(test_parse_header);
    RUN(test_parse_header_too_short);
    RUN(test_parse_question_record);
    RUN(test_parse_answer_a_record);
    RUN(test_get_ipv4);
    RUN(test_nxdomain_response);
    RUN(test_build_parse_roundtrip);
    RUN(test_decode_pointer_to_a_name_with_no_root_label);
    RUN(test_decode_labels_then_pointer_to_a_name_with_no_root_label);
    RUN(test_decode_label_with_no_root_label);
    RUN(test_decode_pointer_to_a_terminated_name);
    RUN(test_record_fields_hold_16_bits);
    RUN(test_record_keeps_a_type_past_one_byte);
    RUN(test_record_keeps_a_class_past_one_byte);
    RUN(test_a_record_still_gives_its_address);
TEST_MAIN_END()
