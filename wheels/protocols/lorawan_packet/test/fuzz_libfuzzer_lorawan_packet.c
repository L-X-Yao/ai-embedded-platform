// SPDX-License-Identifier: MIT
// LibFuzzer harness for lorawan_packet -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_lorawan_packet.c ../lorawan_packet.c -o fuzz_lorawan_packet
// Run:     ./fuzz_lorawan_packet -max_total_time=60 corpus_lorawan_packet/

#include "../lorawan_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);

    // Parse data frame
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    lorawan_parse_data(data, len, &frame);

    // Parse join request (needs at least 23 bytes on wire)
    if (size >= 23) {
        uint8_t app_eui[8], dev_eui[8], dev_nonce[2];
        memset(app_eui, 0, sizeof(app_eui));
        memset(dev_eui, 0, sizeof(dev_eui));
        memset(dev_nonce, 0, sizeof(dev_nonce));
        lorawan_parse_join_request(data, len, app_eui, dev_eui, dev_nonce);
    }

    // Exercise MHDR helpers with each byte
    for (size_t i = 0; i < size && i < 256; i++) {
        lorawan_get_mtype(data[i]);
    }

    // Exercise frame building -- use fuzz data to populate fields
    if (size >= 12) {
        lorawan_frame_t build_frame;
        memset(&build_frame, 0, sizeof(build_frame));
        build_frame.mtype = (lorawan_mtype_t)(data[0] % 6);
        build_frame.dev_addr = (uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                               ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
        build_frame.fcnt = (uint32_t)data[5] | ((uint32_t)data[6] << 8);
        build_frame.fport = data[7];
        build_frame.ack = data[8] & 1;
        build_frame.adr = data[9] & 1;
        build_frame.payload = data + 10;
        build_frame.payload_len = (uint16_t)(size - 10 > 200 ? 200 : size - 10);
        memcpy(build_frame.mic, data + (size >= 14 ? 10 : 0), 4);

        uint8_t build_buf[300];
        lorawan_build_data(build_buf, sizeof(build_buf), &build_frame);
    }

    // Exercise join request building
    if (size >= 18) {
        uint8_t jr_buf[30];
        lorawan_build_join_request(jr_buf, sizeof(jr_buf),
                                   data, data + 8, data + 16);
    }

    // Exercise lorawan_mhdr builder
    lorawan_mhdr(LORAWAN_UNCONFIRMED_UP, 0);
    lorawan_mhdr(LORAWAN_CONFIRMED_DOWN, 1);

    return 0;
}
