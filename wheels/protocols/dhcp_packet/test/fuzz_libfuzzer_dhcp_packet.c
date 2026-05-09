// SPDX-License-Identifier: MIT
// LibFuzzer harness for dhcp_packet — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_dhcp_packet.c ../dhcp_packet.c -o fuzz_dhcp_packet
// Run:     ./fuzz_dhcp_packet -max_total_time=60 corpus_dhcp_packet/

#include "../dhcp_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Parse header */
    dhcp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    dhcp_packet_parse(data, size, &hdr);

    /* Iterate all options */
    if (size >= 240) {
        size_t offset = 240;
        dhcp_option_t opt;
        for (int i = 0; i < 256; i++) {
            int rc = dhcp_packet_next_option(data, size, &offset, &opt);
            if (rc <= 0) break;
        }
    }

    /* Try getting specific options */
    dhcp_option_t opt;
    dhcp_packet_get_option(data, size, DHCP_OPT_MSG_TYPE, &opt);
    dhcp_packet_get_option(data, size, DHCP_OPT_SUBNET_MASK, &opt);
    dhcp_packet_get_option(data, size, DHCP_OPT_ROUTER, &opt);

    return 0;
}
