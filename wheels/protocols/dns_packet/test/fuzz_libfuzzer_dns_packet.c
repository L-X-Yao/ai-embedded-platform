// SPDX-License-Identifier: MIT
// LibFuzzer harness for dns_packet -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_dns_packet.c ../dns_packet.c -o fuzz_dns_packet
// Run:     ./fuzz_dns_packet -max_total_time=60 corpus_dns_packet/

#include "../dns_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);

    // Parse DNS header
    dns_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dns_err_t err = dns_parse_header(data, len, &hdr);

    // If header parsed, try parsing records
    if (err == DNS_OK) {
        uint16_t offset = DNS_HEADER_SIZE;

        // Parse question records
        for (uint16_t i = 0; i < hdr.qdcount && i < 8; i++) {
            dns_record_t rec;
            memset(&rec, 0, sizeof(rec));
            int result = dns_parse_record(data, len, offset, &rec, false);
            if (result < 0) break;
            offset = (uint16_t)result;
        }

        // Parse answer records
        for (uint16_t i = 0; i < hdr.ancount && i < 8; i++) {
            dns_record_t rec;
            memset(&rec, 0, sizeof(rec));
            int result = dns_parse_record(data, len, offset, &rec, true);
            if (result < 0) break;
            offset = (uint16_t)result;

            // Try extracting IPv4 from A records
            uint8_t ip[4];
            dns_get_ipv4(&rec, ip);
        }
    }

    // Exercise domain name decoding with fuzz data
    if (size >= DNS_HEADER_SIZE) {
        char name[DNS_NAME_MAX + 1];
        memset(name, 0, sizeof(name));
        dns_decode_name(data, len, DNS_HEADER_SIZE, name, sizeof(name));
    }

    // Exercise domain name encoding -- needs NUL-terminated string
    {
        size_t copy_len = size < 252 ? size : 252;
        char name_str[253];
        memcpy(name_str, data, copy_len);
        name_str[copy_len] = '\0';

        uint8_t enc_buf[512];
        dns_encode_name(name_str, enc_buf, sizeof(enc_buf));
    }

    // Exercise query building
    {
        size_t copy_len = size < 252 ? size : 252;
        char name_str[253];
        memcpy(name_str, data, copy_len);
        name_str[copy_len] = '\0';

        uint8_t query_buf[512];
        dns_build_query(query_buf, sizeof(query_buf), 0x1234,
                        name_str, DNS_TYPE_A);
    }

    return 0;
}
