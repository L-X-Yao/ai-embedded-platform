// SPDX-License-Identifier: MIT
// LibFuzzer harness for mqtt_packet — coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_mqtt_packet.c ../mqtt_packet.c -o fuzz_mqtt_packet
// Run:     ./fuzz_mqtt_packet -max_total_time=60 corpus_mqtt_packet/

#include "../mqtt_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Exercise all parse functions with fuzz input

    // Get packet type
    mqtt_packet_get_type(data, size);

    // Parse CONNACK
    mqtt_connack_t connack;
    memset(&connack, 0, sizeof(connack));
    mqtt_packet_parse_connack(data, size, &connack);

    // Parse PUBLISH
    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    mqtt_packet_parse_publish(data, size, &pub);

    // Parse PUBACK
    uint16_t packet_id;
    mqtt_packet_parse_puback(data, size, &packet_id);

    // Decode remaining length
    int bytes_used = 0;
    mqtt_packet_decode_remaining_length(data, size, &bytes_used);

    return 0;
}
