// SPDX-License-Identifier: MIT
/*
 * prop_dns_packet.c — property-based tests for dns_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_dns_packet.c ../dns_packet.c -o prop_dns_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../dns_packet.h"
#include <string.h>

/* Property: build query then parse header roundtrip */
PROPERTY(query_roundtrip, 10000) {
    /* Generate a valid-looking domain: one or two labels of alpha chars */
    char name[64];
    uint8_t label1_len = (uint8_t)ARBI_RANGE(seed, 1, 20);
    uint8_t pos = 0;
    for (uint8_t i = 0; i < label1_len; i++)
        name[pos++] = (char)ARBI_RANGE(seed, 'a', 'z');
    name[pos++] = '.';
    uint8_t label2_len = (uint8_t)ARBI_RANGE(seed, 2, 6);
    for (uint8_t i = 0; i < label2_len; i++)
        name[pos++] = (char)ARBI_RANGE(seed, 'a', 'z');
    name[pos] = '\0';

    uint16_t id = ARBI_UINT16(seed);
    uint8_t buf[512];
    int built = dns_build_query(buf, sizeof(buf), id, name, DNS_TYPE_A);
    PROP_ASSERT(built > 0);

    dns_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dns_err_t err = dns_parse_header(buf, (uint16_t)built, &hdr);
    PROP_ASSERT_EQ(err, DNS_OK);
    PROP_ASSERT_EQ(hdr.id, id);
    PROP_ASSERT_EQ(hdr.is_response, false);
    PROP_ASSERT_EQ(hdr.qdcount, 1);
}

/* Property: name encode is deterministic */
PROPERTY(name_encode_determinism, 5000) {
    char name[32];
    uint8_t len = (uint8_t)ARBI_RANGE(seed, 3, 20);
    for (uint8_t i = 0; i < len; i++)
        name[i] = (char)ARBI_RANGE(seed, 'a', 'z');
    name[len] = '\0';

    uint8_t buf1[256], buf2[256];
    int len1 = dns_encode_name(name, buf1, sizeof(buf1));
    int len2 = dns_encode_name(name, buf2, sizeof(buf2));
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[512];
    size_t len = ARBI_SIZE(seed, 512);
    ARBI_FILL(seed, garbage, len);

    dns_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dns_parse_header(garbage, (uint16_t)(len > 65535 ? 65535 : len), &hdr);

    dns_record_t rec;
    memset(&rec, 0, sizeof(rec));
    dns_parse_record(garbage, (uint16_t)(len > 65535 ? 65535 : len),
                     DNS_HEADER_SIZE, &rec, false);

    char decoded[DNS_NAME_MAX];
    dns_decode_name(garbage, (uint16_t)(len > 65535 ? 65535 : len),
                    0, decoded, sizeof(decoded));
}

TEST_MAIN("dns_packet properties") {
    RUN(query_roundtrip);
    RUN(name_encode_determinism);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
