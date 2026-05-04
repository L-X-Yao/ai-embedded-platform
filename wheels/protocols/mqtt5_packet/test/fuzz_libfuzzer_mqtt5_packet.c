// SPDX-License-Identifier: MIT
// LibFuzzer harness for mqtt5_packet — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_mqtt5_packet.c ../mqtt5_packet.c -o fuzz_mqtt5_packet
// Run:     ./fuzz_mqtt5_packet -max_total_time=60 corpus_mqtt5_packet/

#include "../mqtt5_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    // Parse CONNACK (v5.0)
    mqtt5_connack_t connack;
    memset(&connack, 0, sizeof(connack));
    mqtt5_parse_connack(data, len, &connack);

    // Parse PUBLISH (v5.0)
    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    mqtt5_parse_publish(data, len, &pub);

    // Decode variable byte integer
    uint32_t value;
    uint8_t bytes_used;
    mqtt5_decode_vbi(data, len, &value, &bytes_used);

    return 0;
}
